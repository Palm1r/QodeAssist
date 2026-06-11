# Agent Templates — Design Note (body model, include, extends)

Status: agreed design / ready to implement. Dev-facing (not end-user docs).
Scope: how agent TOML profiles describe the request and share structure.

## Problem this replaces

The shipped model has each agent embed a `[template].message_format` jinja string
that hand-builds the **whole** request body as text, plus `[template.sampling]` and
`[template.thinking.*]` blocks merged in by `applySampling`. Pains:

- Massive copy-paste: 9 OpenAI-compatible agents share a byte-identical ~50-line
  `message_format`; 4 Claude agents share another; `role` + README `context` are
  identical across 18 files.
- `[template.sampling]` / `[template.thinking.overrides]` /
  `[template.thinking.request_block.*]` describe **merge machinery**, not the request
  body — they don't look like the actual API call. The `overrides` vs `request_block`
  split is meaningless (both are deep-merged into the request identically).
- Manual JSON-by-string-concatenation: trailing-comma bookkeeping
  (`{% if not loop.is_last %},{% endif %}`) everywhere; a missing comma fails
  silently at runtime (`renderBody` returns nullopt, only a `qWarning`).
- `include` is hard-disabled, so there is no way to share a sub-fragment.

## Agreed model

### 1. `[body]` is a deep-mergeable table = the request body, 1:1 with the API

Replace the `message_format` string and the `sampling`/`thinking` blocks with a
single `[body]` TOML table whose keys are the **literal request-body fields**.
Because it is a table (not a string), `extends` / `deepMerge` can override it
field-by-field — variants become a 2-line delta instead of a copied body.

Field-value rules at build time (per key in `[body]`, applied recursively):
- **string containing jinja** (`{{` or `{%`) → render through inja, splice the
  output as **raw JSON** (array / object / string). Empty render → key omitted.
- **string without jinja** (e.g. `"high"`) → literal JSON string, as-is.
- **number / bool / inline-table** → as-is.

So `messages` / `contents` and `system` / `system_instruction` are just **string
fields holding jinja**; everything else (`max_tokens`, `temperature`, `stream`,
`thinking`, `output_config`, `generationConfig`, …) is a literal value that reads
exactly like the curl body.

No runtime toggles: thinking / tools / streaming are **fixed per agent**. A thinking
agent literally carries the `thinking` fields; a non-thinking variant is a separate
file. There is no `{% if thinking %}` in the body. `system` uses
`{% if existsIn(ctx, "system_prompt") %}` only because that is about *presence of
data*, not a mode toggle. `enable_thinking` / `enable_tools` are **capability hints**
(used for UI badges and to decide tool-definition injection) — the body is the source
of truth for what is actually sent, so a thinking agent's body must carry the thinking
fields regardless of the flag.

Outside the body:
- `model` — the TOML `model` is the **default**; a per-agent override chosen in
  QodeAssist settings wins. Overrides are stored in `agent_models.json`
  (agentName → model) and applied by `AgentFactory` when it builds the agent
  (`AgentFactory::effectiveModel`/`setModelOverride`); `Session` still seeds the
  payload `model` from the resolved `cfg.model`. URL-model providers (Google) put a
  `${MODEL}` placeholder in `endpoint`; `Session` substitutes the resolved model into
  the endpoint before sending (same substitution style as `${PROJECT_DIR}`/`${HOME}`),
  so the override drives the URL too.
- `tools` — injected by the **provider** when `enable_tools` is set (tool
  definitions are dynamic, from `ToolsManager`; they can't be authored in TOML).
- `stream` — always on. Literal `"stream": true` in the body for OpenAI / Claude /
  Mistral / Responses / Ollama; encoded in the `endpoint` URL for Google.

### 2. `include` re-enabled as whitelisted partials

The message-array rendering (the complex, comma-heavy part) lives in
`sources/agents/partials/*.jinja`, shared via `{% include %}`. The throwing include
callback is replaced by a sandboxed resolver that:
- rejects names containing `..`, a leading `/`, or a scheme/drive;
- resolves only against known roots: bundled `:/agents/partials/` then the user
  `partials/` dir;
- parses/caches the partial in the same `inja::Environment`.

A missing/typo'd partial is a **load-time** error.

### 3. `extends` shares config down a hierarchy

`extends` already exists (`resolveExtends` + `deepMerge` + `abstract`/`hidden`); it
keeps doing what it does, now over the structured `[body]` too. Each API-shape base
sets `system_prompt = """{{ agent_role() }}"""` (the role text comes from the role
JSON via the `agent_role` callback; see below). No shared root base. Between the
API-shape base and the concrete agents sits one thin abstract base **per provider**
(provider_instance + endpoint only) — the designated extension point for user
agents, so a custom agent is `extends` + `name` + `model`:

```
openai_base (abstract)        → system_prompt + [body]   (API shape)
  ├─ mistral_base (abstract)  → provider, endpoint       (per-provider)
  │   ├─ mistral_chat         → name, model
  │   └─ mistral_reasoning    → name, model + enable_thinking
  ├─ openrouter_base (abstract) ...
  └─ openai_chat              → name, model              (own provider = no mid layer)
anthropic_base (abstract)     → system_prompt + provider/endpoint + [body]
  └─ claude_sonnet46          → name, model + [body] thinking / output_config
google_base (abstract)        → system_prompt + provider + [body]
  └─ gemini_chat              → endpoint (${MODEL}) + [body.generationConfig] thinkingConfig
```

Bundled agents are read-only: the loader rejects a user file that reuses a bundled
`name`. Customisation = a user agent under a new name extending a bundled base (or a
concrete bundled agent); the per-agent model override in settings covers the
model-only case without any file.

Notes:
- `[body]` is shared whole when identical (the 8 OpenAI-compatible providers); a
  variant overrides only the differing field — no duplicated body.
- Arrays (`tags`) are **replaced** on override, not appended (`deepMerge` recurses
  objects only). A child that wants base tags + extras restates the full list.
- Division of labour: **include** shares the message-rendering fragment across
  unrelated families; **extends** shares config (system_prompt / endpoint / body)
  down one inheritance chain.
- With `model` gone, per-model files collapse: agents that previously differed only
  by `model` become one agent (the client picks the model). A separate file is only
  needed when the body genuinely differs (effort, no-thinking, …).

### System prompt — a composable template with building blocks

The old `role` (static text) and `context` (jinja) layers collapse into one
`agent.system` layer in `Session`, rendered through `ContextRenderer`. The agent's
`system_prompt` field IS that template, and the user edits it (in the profile) to
compose the prompt from building-block callbacks:

- `{{ agent_role("<id>") }}` — insert a role's text (Developer/Reviewer/Researcher…).
  Implemented as a `ContextRenderer` callback (`registerAgentRole`) that reads
  `userResourcePath("qodeassist/agent_roles/<id>.json")["systemPrompt"]`. Returns "" if
  missing. Lives in `sources/agents` (no dependency on `settings/`), so it works in the
  plugin and bench. The role text lives once in the role JSON (managed by the settings
  Roles UI); the chat bases just carry `system_prompt = """{{ agent_role("developer") }}"""`.
- `{{ read_file("...") }}` / `file_exists` / `${PROJECT_DIR}` / `${HOME}` — existing
  `ContextRenderer` helpers, composable in the same template.

So a profile can do `system_prompt = """{{ agent_role("developer") }}\n\n{{ read_file("…") }}"""`.
`qodeassist.cpp` calls `AgentRolesManager::ensureDefaultRoles()` at startup so the default
role JSONs exist before agents load. There is NO per-agent settings override — the edit
point is the profile's `system_prompt`. Code-completion/FIM agents set no `system_prompt`.

## Worked examples

OpenAI base:
```toml
abstract = true
system_prompt = """{{ agent_role("developer") }}"""
provider_instance = "OpenAI (Chat Completions)"
endpoint = "/chat/completions"
enable_tools = true

[body]
max_tokens  = 8192
temperature = 0.7
stream      = true
messages    = """
[ {% include "partials/openai_messages.jinja" %} ]
"""
```

Mistral reasoning child (delta only):
```toml
extends = "OpenAI Base Chat"
name    = "Mistral Reasoning Chat"
provider_instance = "Mistral AI"
endpoint = "/v1/chat/completions"
enable_thinking = true

[body]
reasoning_effort = "medium"
```

Claude base (literally the curl body):
```toml
abstract = true
system_prompt = """{{ agent_role("developer") }}"""
provider_instance = "Claude"
endpoint = "/v1/messages"
enable_thinking = true
enable_tools = true

[body]
max_tokens  = 16000
temperature = 1
stream      = true
thinking      = { type = "adaptive", display = "summarized" }
output_config = { effort = "high" }
system   = """{% if existsIn(ctx, "system_prompt") %}{{ tojson(ctx.system_prompt) }}{% endif %}"""
messages = """
[ {% include "partials/anthropic_messages.jinja" %} ]
"""
```

Sonnet child (delta only):
```toml
extends = "Anthropic Base Chat"
name    = "Claude Sonnet"

[body.output_config]
effort = "medium"
```

Google base (`${MODEL}` in endpoint; streaming in the URL):
```toml
abstract = true
system_prompt = """{{ agent_role("developer") }}"""
provider_instance = "Google AI"
endpoint = "/models/${MODEL}:streamGenerateContent?alt=sse"
enable_thinking = true
enable_tools = true

[body]
system_instruction = """{% if existsIn(ctx, "system_prompt") %}{ "parts": [ { "text": {{ tojson(ctx.system_prompt) }} } ] }{% endif %}"""
contents = """
[ {% include "partials/google_contents.jinja" %} ]
"""

[body.generationConfig]
maxOutputTokens = 16000
temperature     = 1
thinkingConfig  = { includeThoughts = true, thinkingBudget = 8192 }
```

### Partials

`partials/openai_messages.jinja` dispatches per message:
```jinja
{% if existsIn(ctx, "system_prompt") %}
{ "role": "system", "content": {{ tojson(ctx.system_prompt) }} },
{% endif %}
{% for msg in ctx.history %}
  {% if msg.role == "assistant" %}{% include "partials/openai_assistant.jinja" %}
  {% else if length(filter_by_type(msg.content_blocks, "tool_result")) > 0 %}{% include "partials/openai_tool_results.jinja" %}
  {% else %}{% include "partials/openai_user.jinja" %}
  {% endif %}
{% endfor %}
```

`partials/openai_assistant.jinja`:
```jinja
{% set tcalls = filter_by_type(msg.content_blocks, "tool_use") %}
{
  "role": "assistant",
  "content": {{ tojson(msg.content) }}
  {% if length(tcalls) > 0 %}
  , "tool_calls": [
    {% for b in tcalls %}
    { "id": {{ tojson(b.id) }}, "type": "function",
      "function": { "name": {{ tojson(b.name) }}, "arguments": {{ tojson(tojson(b.input)) }} } },
    {% endfor %}
  ]
  {% endif %}
},
```

`partials/openai_tool_results.jinja`:
```jinja
{% for b in filter_by_type(msg.content_blocks, "tool_result") %}
{ "role": "tool", "tool_call_id": {{ tojson(b.tool_use_id) }}, "content": {{ tojson(b.content) }} },
{% endfor %}
```

`partials/openai_user.jinja`:
```jinja
{% if existsIn(msg, "images") %}
{ "role": "user", "content": {% include "partials/openai_image_content.jinja" %} },
{% else %}
{ "role": "user", "content": {{ tojson(msg.content) }} },
{% endif %}
```

`partials/openai_image_content.jinja`:
```jinja
[
  { "type": "text", "text": {{ tojson(msg.content) }} }
  {% for img in msg.images %}
  ,
  {% if img.is_url %}
  { "type": "image_url", "image_url": { "url": {{ tojson(img.data) }} } }
  {% else %}
  { "type": "image_url", "image_url": { "url": "data:{{ img.media_type }};base64,{{ img.data }}" } }
  {% endif %}
  {% endfor %}
]
```

`partials/anthropic_messages.jinja`:
```jinja
{% for msg in ctx.history %}
{
  "role": {{ tojson(msg.role) }},
  "content": [
    {% for b in msg.content_blocks %}
      {% if b.type == "image" %}{% include "partials/anthropic_image.jinja" %}
      {% else %}{{ tojson(b) }},
      {% endif %}
    {% endfor %}
  ]
},
{% endfor %}
```

`partials/anthropic_image.jinja`:
```jinja
{
  "type": "image",
  "source":
  {% if b.is_url %}
  { "type": "url", "url": {{ tojson(b.data) }} }
  {% else %}
  { "type": "base64", "media_type": {{ tojson(b.media_type) }}, "data": {{ tojson(b.data) }} }
  {% endif %}
},
```

`partials/google_contents.jinja`:
```jinja
{% for msg in ctx.history %}
{
  "role": {% if msg.role == "assistant" %}"model"{% else %}"user"{% endif %},
  "parts": [ {% for b in msg.content_blocks %}{% include "partials/google_part.jinja" %}{% endfor %} ]
},
{% endfor %}
```

`partials/google_part.jinja`:
```jinja
{% if b.type == "text" %}
{ "text": {{ tojson(b.text) }} },
{% else if b.type == "thinking" %}
{ "text": {{ tojson(b.thinking) }}, "thought": true, "thoughtSignature": {{ tojson(b.signature) }} },
{% else if b.type == "tool_use" %}
{ "functionCall": { "name": {{ tojson(b.name) }}, "args": {{ tojson(b.input) }} } },
{% else if b.type == "tool_result" %}
{ "functionResponse": { "name": {{ tojson(b.name) }}, "response": { "result": {{ tojson(b.content) }} } } },
{% else if b.type == "image" %}
  {% if b.is_url %}
  { "file_data": { "mime_type": {{ tojson(b.media_type) }}, "file_uri": {{ tojson(b.data) }} } },
  {% else %}
  { "inline_data": { "mime_type": {{ tojson(b.media_type) }}, "data": {{ tojson(b.data) }} } },
  {% endif %}
{% else %}
{ "text": "" },
{% endif %}
```

## C++ work

In `JsonPromptTemplate`:
- Parse `[body]` as a `QJsonObject` (not a string). Walk it recursively and build the
  request: render jinja-bearing string values via inja and splice the parsed JSON;
  pass literal strings / scalars / inline-tables through; drop keys whose render is
  empty.
- **Delete** `m_sampling`, `m_thinking`, and `applySampling` entirely — the body is
  the request; there is no separate sampling/thinking merge.
- Drop the `thinkingEnabled` parameter from `buildFullRequest` /
  `Provider::prepareRequest` / `Session` — it no longer affects rendering.
- Add a **JSON-aware** trailing-comma stripper before `QJsonDocument::fromJson`
  (tracks string/escape state so `,}` / `,]` inside string values are not touched).
  This is what lets partials emit an unconditional `,` after every element and drop
  all `loop.is_last` bookkeeping.

In `AgentConfig` / `AgentLoader`:
- Replace `messageFormat` (string) with `body` (`QJsonObject`); merge `role` +
  `context` into `system_prompt`. `[template].sampling` / `[template].thinking` are
  removed.
- `extends` / `deepMerge` are unchanged; they now also merge `[body]`.
- Validate at load: a referenced partial must resolve; the assembled body must parse
  as JSON (render once against a synthetic context with tool_use / tool_result /
  image). Catches breakage at startup, not mid-conversation.

Model selection (per-agent override):
- `AgentFactory` owns an agentName → model map loaded from `agent_models.json`
  (`loadModelOverrides`/`saveModelOverrides`). `create()`/`createFromFile()` apply the
  override into the built `AgentConfig`; `effectiveModel()` exposes the resolved value;
  `setModelOverride()` persists. The settings UI (`AgentDetailPane`) edits it via an
  editable Model field; list/roster widgets display `effectiveModel`.
- `Session` substitutes `${MODEL}` in `cfg.endpoint` with the resolved model before
  `sendRequest` (covers Google, whose model lives in the URL), and still seeds the
  payload `model` from `cfg.model`. The provider keeps injecting `tools` when
  `enable_tools` is set.

In `Session`:
- Collapse the `agent.role` + `agent.context` system-prompt layers into one rendered
  `system_prompt` layer.

## Implementation order

1. JSON-aware trailing-comma stripper + whitelisted `include` resolver (enables
   readable partials).
2. `[body]`-table model in `JsonPromptTemplate` + loader; delete
   sampling/thinking/`applySampling`; drop `thinkingEnabled`.
3. `system_prompt` merge in loader + `Session`.
4. Per-agent model override in `AgentFactory` (`agent_models.json`) + `${MODEL}`
   endpoint substitution in `Session`; editable Model field in settings; convert
   bundled agents to the base/partials/`extends` layout.
5. Load-time validation (partial resolves, body parses).
