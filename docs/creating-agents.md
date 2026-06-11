# Creating and Extending Agents

An *agent* is a TOML profile that tells QodeAssist which provider to call,
which model to use, and exactly what request body to send. All bundled agents
(Settings → QodeAssist → Agents) are built from the same files described here —
anything a bundled agent does, a user agent can do too.

## Where user agents live

Drop `*.toml` files into the user agents directory:

| OS | Path |
|---|---|
| Linux / macOS | `~/.config/QtProject/qtcreator/qodeassist/config/agents/` |
| Windows | `%APPDATA%\QtProject\qtcreator\qodeassist\config\agents\` |

QodeAssist creates the directory on startup. Files are loaded at plugin
startup; after adding or editing a file, restart Qt Creator.

Two layers are loaded:

1. **Bundled** agents shipped inside the plugin — read-only.
2. **User** agents from the directory above (marked with a `user` pill).

Agent `name`s are global across both layers. A user file that reuses a
bundled agent's `name` is rejected with an error — bundled agents cannot be
replaced; create your own agent under a new name and `extends` what you want
to build on. Two *user* files with the same `name` produce a warning, and
the alphabetically later file wins.

Load errors and warnings (TOML syntax, unknown keys, missing `extends`
parents, bodies that don't render to valid JSON) are reported in Qt Creator's
**General Messages** pane, prefixed with `[Agents]`.

## Minimal example

```toml
schema_version = 1

name        = "My DeepSeek Chat"
description = "DeepSeek V3 via an OpenAI-compatible endpoint."

provider_instance = "OpenAI Compatible"
model             = "deepseek-chat"
endpoint          = "/chat/completions"

system_prompt = """{{ agent_role() }}"""

[body]
max_tokens  = 8192
temperature = 0.7
stream      = true
messages    = """
[ {% include "partials/openai_messages.jinja" %} ]
"""
```

Shorter still — extend a bundled provider base and state only the delta:

```toml
schema_version = 1

extends = "OpenAI Compatible Base Chat"
name    = "My DeepSeek Chat"
model   = "deepseek-chat"
```

Bundled agents themselves are read-only. To get a variant of a preset, create
your own agent under a new name and put it where you want it in the roster:

```toml
schema_version = 1

extends = "Mistral Base Chat"
name    = "My Mistral (low temp)"
model   = "mistral-small-latest"

[body]
temperature = 0.3
```

If all you want is a different model for a preset, you don't need a file at
all — set the per-agent model override in the settings UI.

## Key reference

| Key | Required | Meaning |
|---|---|---|
| `schema_version` | no (default 1) | Format version; the plugin refuses files newer than it supports. |
| `name` | yes | Unique identifier; shown in the UI, referenced by rosters and `extends`. |
| `description` | no | Tooltip text in the Agents list. |
| `provider_instance` | yes* | Name of a provider instance (see below). |
| `model` | yes* | Default model; can be overridden per agent in settings. |
| `endpoint` | yes* | Path appended to the provider instance URL. May contain `${MODEL}` (e.g. Google: `/models/${MODEL}:streamGenerateContent?alt=sse`). |
| `system_prompt` | no | Jinja template for the system prompt (see building blocks below). FIM agents usually omit it. |
| `tags` | no | Free-form strings shown as pills in the UI for discoverability. |
| `enable_thinking` | no | Capability hint (UI badge). The `[body]` is the source of truth for what is sent. |
| `enable_tools` | no | Lets the provider inject tool definitions into the request. |
| `cache_prompt` / `cache_ttl` | no | Prompt caching (Anthropic); `cache_ttl = "1h"` selects the long TTL. |
| `extends` | no | Name of a parent agent to inherit from. |
| `abstract` | no | Mark as template-only: it can be extended but is never loaded as a usable agent. Not inherited. |
| `hidden` | no | Loaded and usable, but not listed in selection UIs. Not inherited. |
| `[match]` | no | Routing constraints (see Routing). |
| `[body]` | yes* | The literal request body (see below). |

\* required after `extends` resolution — a child inherits these from its
parent, so it only states what differs.

### Required keys checked at load

A concrete (non-abstract) agent must end up with `name`,
`provider_instance`, `model`, `endpoint`, and a non-empty `[body]`. Unknown
keys anywhere at the top level or in `[match]` produce a warning — this
catches typos like `enable_thinkin`.

## Provider instances

`provider_instance` refers to a provider configuration (URL + API key
reference + client API). Bundled instances:

`Claude`, `Codestral`, `Google AI`, `llama.cpp`,
`LM Studio (Chat Completions)`, `LM Studio (Responses API)`, `Mistral AI`,
`Ollama (Native)`, `Ollama (OpenAI-compatible)`, `OpenAI (Chat Completions)`,
`OpenAI (Responses API)`, `OpenAI Compatible`, `OpenRouter`.

User-defined instances live next to agents in
`…/qodeassist/config/providers/*.toml` and follow the same
override-by-name layering.

## `extends` — inheriting from another agent

A child deep-merges over its parent: scalar keys are replaced, tables (such
as `[body]` and `[body.options]`) merge key-by-key, and **arrays are replaced
whole** (a child that wants the parent's `tags` plus one more must restate
the full list). Chains can be deeper than one level; cycles and missing
parents are load errors.

`abstract` and `hidden` are never inherited — extending a hidden agent
yields a visible child unless the child says otherwise.

Every provider ships an abstract base that already carries the provider
instance, endpoint, and request body — extending one and setting `model` is
usually all a custom agent needs:

| Base | Provider / API |
|---|---|
| `Anthropic Base Chat` | Claude, Anthropic Messages (`/v1/messages`) |
| `OpenAI Base Chat` | OpenAI, Chat Completions (`/chat/completions`) |
| `OpenAI Responses Base` | OpenAI, Responses API (`/responses`) |
| `OpenAI Compatible Base Chat` | Any OpenAI-compatible server |
| `Google Base Chat` | Google AI, Gemini `generateContent` |
| `Mistral Base Chat` | Mistral AI, Chat Completions |
| `Codestral Base Chat` | Codestral, Chat Completions |
| `Codestral FIM Base` | Mistral AI, `/v1/fim/completions` code completion |
| `OpenRouter Base Chat` | OpenRouter, Chat Completions |
| `LM Studio Base Chat` | LM Studio, Chat Completions |
| `LM Studio Responses Base` | LM Studio, Responses API |
| `llama.cpp Base Chat` | llama.cpp server, Chat Completions |
| `Ollama Base Chat` | Ollama, native `/api/chat` |
| `Ollama (OpenAI-compatible) Base Chat` | Ollama, OpenAI-compatible endpoint |
| `Ollama FIM Base` | Ollama, native `/api/generate` fill-in-the-middle |

Concrete agents work as parents too — `extends = "Claude Sonnet 4.6 Chat"`
inherits everything including the model.

## `[body]` — the request, literally

`[body]` is the request body, written exactly like the provider's curl
example. Per key, recursively:

- **string containing jinja** (`{{` or `{%`) — rendered, and the output is
  spliced in as raw JSON. A render that produces nothing drops the key.
- **plain string / number / bool / table** — passed through unchanged.

```toml
[body]
max_tokens  = 16000
stream      = true
thinking    = { type = "adaptive", display = "summarized" }
messages    = """
[ {% include "partials/anthropic_messages.jinja" %} ]
"""
```

There are no runtime toggles: a thinking variant is a separate agent file
that carries the thinking fields in its body.

Every agent body is dry-run rendered at load against a synthetic
conversation (text, thinking, tool calls, tool results, images), so jinja
syntax errors, unknown callbacks, missing partials, and invalid JSON are
reported at startup — not mid-conversation. Trailing commas emitted by loops
are stripped automatically; don't bother with `loop.is_last` bookkeeping.

### Template data (`ctx`)

| Field | Content |
|---|---|
| `ctx.system_prompt` | Rendered system prompt (present only if the agent has one). |
| `ctx.prefix` / `ctx.suffix` | Code around the cursor (FIM/completion sessions). |
| `ctx.files_metadata` | Array of `{ file_path, content }` for attached files. |
| `ctx.history` | Array of messages: `{ role, content, content_blocks, images? }`. |

`content` is the message's flattened text; `content_blocks` is the
structured form:

| `type` | Fields |
|---|---|
| `text` | `text` |
| `thinking` | `thinking`, `signature` |
| `redacted_thinking` | `data` |
| `tool_use` | `id`, `name`, `input` (JSON object) |
| `tool_result` | `tool_use_id`, `content`, `name` |
| `image` | `data`, `media_type`, `is_url` |

### Callbacks available in `[body]`

| Callback | Purpose |
|---|---|
| `tojson(x)` | Serialize any value as JSON (correct quoting/escaping). Use it for every interpolated value. |
| `filter_by_type(blocks, "tool_use")` | Subset of `content_blocks` with the given type. |
| `filter_skip_role(history, "system")` | History without messages of a role. |
| `strip_signature_suffix(s)` | Remove a trailing `[Signature: …]` marker. |

### Partials and `{% include %}`

The repetitive message-array rendering lives in shared partials. Includes
resolve against the bundled set first, then the user agent's own directory —
so a user agent can both reuse bundled partials and ship its own next to its
TOML (e.g. `partials/my_messages.jinja`). Paths with `..` or a leading `/`
are rejected.

Bundled partials: `partials/openai_messages.jinja`,
`partials/openai_responses_input.jinja`, `partials/anthropic_messages.jinja`,
`partials/google_contents.jinja`, `partials/ollama_messages.jinja` (plus the
per-message helpers they include).

## `system_prompt` — composable building blocks

`system_prompt` is itself a jinja template, rendered with:

| Helper | Purpose |
|---|---|
| `{{ agent_role() }}` | Text of the role currently selected in the chat (falls back to `developer`). |
| `{{ agent_role("reviewer") }}` | A specific role by id (Settings → QodeAssist → Roles). |
| `{{ read_file("${PROJECT_DIR}/STYLE.md") }}` | Inline a file. Reads are restricted to the project directory and `~/qodeassist`. |
| `{{ file_exists(p) }}` / `{{ read_dir(p) }}` | Existence check / directory listing (same root restrictions). |
| `{{ head_lines(s, n) }}` | First `n` lines of a string. |
| `basename`, `dirname`, `ext`, `lower`, `upper` | Path/string helpers. |
| `${PROJECT_DIR}`, `${HOME}` | Substituted before rendering. |

Example:

```toml
system_prompt = """
{{ agent_role() }}

{% if file_exists("${PROJECT_DIR}/.qodeassist-style.md") %}
Project conventions:
{{ read_file("${PROJECT_DIR}/.qodeassist-style.md") }}
{% endif %}
"""
```

## Routing — `[match]` and rosters

Each pipeline (code completion, chat, compression, quick refactor) has an
ordered roster of agents. For the current file, the **first roster entry
whose `[match]` accepts** wins.

```toml
[match]
file_patterns = ["*.qml", "*.js"]
path_patterns = ["*/tests/*"]
project_names = ["MyProject"]
```

- Dimensions are ANDed; an empty dimension is unconstrained; an entirely
  empty/absent `[match]` is a catch-all.
- `file_patterns` are case-insensitive globs tested against the file name
  and the full path; `path_patterns` against the full path only.
- `project_names` are exact, case-sensitive project names.

Typical setup: a specialized agent (e.g. `Qt CodeLlama 13B QML FIM` with
`*.qml`) first, a catch-all agent last.

## Models

The TOML `model` is only the default. The settings UI can set a per-agent
override (stored in `agent_models.json`); the resolved model is also
substituted into `${MODEL}` in `endpoint` before sending.

## Contributing your agent to QodeAssist

The bundled agent set grows through contributions — if you've made an agent
for a provider or model that others could use, please send it upstream
instead of keeping it local. No C++ is needed:

1. Develop and verify the agent locally in the user agents directory.
2. In a fork, copy the TOML to `sources/agents/` and register the file in
   `sources/agents/agents.qrc`.
3. Keep it a thin delta: extend the matching provider base and set only
   `name`, `description`, `model`, `tags` (and `[body]` keys that genuinely
   differ). Look at `mistral_chat.toml` or `ollama_qwen25_coder_fim.toml`
   for the expected shape.
4. Run the tests (`QodeAssistTest`): `BundledAgentsTest` automatically
   loads every bundled agent, resolves its `extends` chain, and dry-renders
   its `[body]` — if your TOML passes, it works.
5. Open a pull request.

Conventions:

- File name: `<provider>_<model_or_purpose>_<kind>.toml`
  (e.g. `ollama_qwen25_coder_fim.toml`).
- `name` is user-visible and must be unique; include the provider and model
  (e.g. `Ollama Qwen2.5-Coder FIM`).
- Specialized completion agents should carry a `[match]` block so routing
  can pick them automatically (e.g. `file_patterns = ["*.qml"]`).
- A whole new provider with an OpenAI-compatible API is also TOML-only: a
  provider instance file in `sources/providersConfig/`, one abstract
  `<Provider> Base Chat`, and concrete agents on top. New request/response
  *formats* are the only thing that needs C++.

## Troubleshooting

- **Agent missing from the list** — check General Messages for `[Agents]
  error:` lines; the file failed to parse, resolve, or validate.
- **`… has the same name as a bundled agent — bundled agents cannot be
  replaced`** — pick a different `name`; use `extends` to inherit from the
  bundled agent instead.
- **`Unknown key 'x' … ignored (typo?)`** — the key isn't part of the
  schema; compare with the table above.
- **`Agent 'X' extends unknown agent 'Y'`** — the parent's `name` (not file
  name) must match exactly; the parent must be bundled or in the same
  directory.
- **`[body] failed to render to valid JSON`** — the dry run failed; the log
  contains the rendered snippet. Usually a missing `tojson(...)` around an
  interpolated string.
- **Edits not picked up** — agents are loaded at startup; restart
  Qt Creator.
