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

A custom agent is a thin delta over a bundled **wire base**: extend it, set the
model, override only what differs. The base already carries the provider, the
endpoint and the request-body serialization — you add the policy.

```toml
schema_version = 1

extends = "Claude Base Chat"
name    = "My Claude"
model   = "claude-sonnet-4-6"
```

Override a body field or the persona:

```toml
schema_version = 1

extends = "Claude Base Chat"
name    = "My Claude (low temp)"
model   = "claude-sonnet-4-6"

system_prompt = """You are a terse code reviewer."""

[body]
temperature = 0.3
```

Point a base at a different OpenAI-compatible provider by overriding the
provider instance and model:

```toml
schema_version = 1

extends           = "OpenAI Base Chat"
name              = "My DeepSeek"
provider_instance = "OpenAI Compatible"
model             = "deepseek-chat"
```

Bundled agents are read-only — vary a preset by creating your own under a new
name. If all you want is a different model, you don't even need a file: set the
per-agent model override in the settings UI.

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
| `cache_breakpoints` | no | Which cache points to set when `cache_prompt` is on: any of `"system"`, `"tools"`, `"history"`. Empty/omitted = all three. |
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

Every provider ships an **abstract wire base** that carries only the provider
instance, endpoint and the request-body serialization — no model, persona,
tags or sampling. Extending one and setting `model` is all a custom agent
needs:

| Base | Provider / API |
|---|---|
| `Claude Base Chat` | Claude, Anthropic Messages (`/v1/messages`) |
| `OpenAI Base Chat` | OpenAI, Chat Completions (`/chat/completions`) |
| `OpenAI Responses Base` | OpenAI, Responses API (`/responses`) |
| `Google Base Chat` | Google AI, Gemini `generateContent` |
| `Ollama Base Chat` | Ollama, native `/api/chat` |
| `Ollama FIM Base` | Ollama, native `/api/generate` fill-in-the-middle |

For any OpenAI-compatible provider (Mistral, OpenRouter, LM Studio, llama.cpp,
DeepSeek, …) extend `OpenAI Base Chat` and override `provider_instance`.

Each bundled concrete agent (`Claude Sonnet Chat`, `Claude Code Completion`,
`OpenAI Chat Completions`, `OpenAI Responses Chat`, `Google Chat`,
`Ollama Chat`, `Ollama FIM`) is itself a thin delta over one of these bases and
works as a parent too — `extends = "Claude Sonnet Chat"` inherits everything including
the model.

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
```

The message-array serialization (`messages` / `contents` / `input`, plus the
`system` renderer) lives in the **wire base**; a concrete agent that extends a
base inherits it and usually sets only scalar policy fields like the ones
above. A from-scratch agent (no `extends`) must carry the full serialization
itself — copy a bundled base's `[body]` as the starting point.

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

The message-array serialization is **inlined directly in each bundled wire
base** — there are no bundled partials to include. The `{% include %}`
mechanism still works for *your own* partials: drop a `partials/*.jinja` next
to your agent TOML and include it with
`{% include "partials/my_messages.jinja" %}`. Includes resolve against the
bundled root first, then the user agent's own directory; paths with `..` or a
leading `/` are rejected.

## `system_prompt` — composable building blocks

`system_prompt` is itself a jinja template, rendered with:

| Helper | Purpose |
|---|---|
| `{{ read_file("${PROJECT_DIR}/STYLE.md") }}` | Inline a file. Reads are restricted to the project directory, your QodeAssist user directory (`${CONFIG_DIR}`), and bundled `:/…` resources. |
| `{{ file_exists(p) }}` / `{{ read_dir(p) }}` | Existence check / directory listing (same root restrictions). |
| `{{ head_lines(s, n) }}` | First `n` lines of a string. |
| `basename`, `dirname`, `ext`, `lower`, `upper` | Path/string helpers. |
| `${PROJECT_DIR}`, `${CONFIG_DIR}` | Substituted before rendering. `${CONFIG_DIR}` is your QodeAssist user directory (where agent configs live). |

Example:

```toml
system_prompt = """
{{ read_file("${CONFIG_DIR}/roles/reviewer.md") }}

{% if file_exists("${PROJECT_DIR}/.qodeassist-style.md") %}
Project conventions:
{{ read_file("${PROJECT_DIR}/.qodeassist-style.md") }}
{% endif %}
"""
```

Reads fail **loud**: a path outside those roots — or a `read_file` whose target
is missing — aborts the request with a clear error instead of silently rendering
an empty prompt. For a genuinely optional file, guard it with `file_exists`,
which returns `false` for an allowed-but-absent path; only a path *outside* the
roots is treated as an authoring error and rejected outright.

The persona is simply what `system_prompt` renders to — inline the text or pull
shared text from a markdown file with `read_file`. The bundled chat agents do
exactly this: their `system_prompt` is `{{ read_file(":/roles/qt-cpp-developer.md") }}`,
reading the shipped role from the plugin resources. To switch personas in the
chat, switch agents: a persona variant is a thin `extends` child that overrides
only `system_prompt` (e.g. pointing `read_file` at any file of your own under
`${CONFIG_DIR}/…` or `${PROJECT_DIR}/…`). `read_file` reads exactly the path
you give it — there is no override convention that swaps a bundled file for a
same-named user file.

## Routing — `[match]` and the completion roster

`[match]` drives **code completion** routing only. Completion has an ordered
roster of agents; for the current file the **first roster entry whose `[match]`
accepts** wins. The other pipelines don't route: chat shows an allow-list of
agents and you pick one in the panel; quick refactor and chat compression each
use a single configured agent (set in QodeAssist → General).

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

Typical completion setup: a specialized agent (e.g. an `Ollama FIM` variant
with `*.qml`) first, a catch-all agent last.

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
   differ). Look at `claude_chat.toml` or `ollama_fim.toml` for the expected
   shape.
4. Run the tests (`QodeAssistTest`): `BundledAgentsTest` automatically
   loads every bundled agent, resolves its `extends` chain, and dry-renders
   its `[body]` — if your TOML passes, it works.
5. Open a pull request.

Conventions:

- File name: `<provider>_<model_or_purpose>_<kind>.toml`
  (e.g. `openrouter_deepseek_chat.toml`).
- `name` is user-visible and must be unique; include the provider and model
  (e.g. `OpenRouter DeepSeek Chat`).
- Specialized completion agents should carry a `[match]` block so routing
  can pick them automatically (e.g. `file_patterns = ["*.qml"]`).
- A new OpenAI-compatible provider is TOML-only: add a provider instance file
  in `sources/providersConfig/`, then a concrete agent that `extends`
  `OpenAI Base Chat` and overrides `provider_instance`. A genuinely new
  request/response *format* (a new wire base) is the only thing that needs C++.

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
