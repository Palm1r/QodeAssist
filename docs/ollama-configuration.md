# Configure for Ollama

1. Install [Ollama](https://ollama.com). Make sure to review the system requirements before installation.
2. Install a language models in Ollama via terminal. For example, you can run:

For standard computers (minimum 8GB RAM):
```bash
ollama run qwen2.5-coder:7b
```
For better performance (16GB+ RAM):
```bash
ollama run qwen2.5-coder:14b
```
For high-end systems (32GB+ RAM):
```bash
ollama run qwen2.5-coder:32b
```

3. Open Qt Creator settings (Edit > Preferences on Linux/Windows, Qt Creator > Preferences on macOS)
4. Navigate to QodeAssist > Providers and verify the bundled **Ollama (Native)** provider points at http://localhost:11434 (no API key needed)
5. Navigate to QodeAssist > General > Agent Pipelines — the Ollama agents are assigned by default:
    - Code completion: **Ollama Completion — FIM** (needs a base/FIM model; use **Ollama Completion — Chat-style** for instruct models)
    - Chat assistant: **Ollama Chat — Simple / Thinking / Gemma 4**
    - Chat compression: **Ollama Compression — 8 GB** (or the 16/32 GB tier for your machine)
    - Quick refactor: **Ollama Quick Refactor — Simple**
6. Point the agents at models you actually have (see the next section)

You're all set! QodeAssist is now ready to use in Qt Creator.

## Which models do I actually need?

You do **not** need a separate model for every agent. Each bundled Ollama agent
names a *default* model only as an example — you can point any agent at a model
you already have via its settings → **Change…** (a per-agent override; it does not
edit the bundled agent). **Seeing a model name on an agent is not a reason to
download it.**

The defaults cluster into a tiny set, so one or two pulls cover everyday use:

| Pull this | Unlocks |
|---|---|
| `qwen2.5-coder:7b` | Ollama Chat — Simple · Ollama Completion — FIM · Ollama Completion — Chat-style · Ollama Quick Refactor |
| `qwen3.5:9b` (or `:4b` on ~8 GB) | Ollama Chat — Thinking · Ollama Compression — 16/32 GB (`:4b` → Compression — 8 GB) |

Optional specialists — pull only if you want that capability:

| Pull this | For |
|---|---|
| `gemma4:12b` | Ollama Chat — Gemma 4 — agentic chat with vision + native reasoning |
| `theqtcompany/codellama-7b-qml` | Ollama Completion — QML (Qt) — Qt's QML-specific completion model |

Rule of thumb: pick the agent for the job, then either pull its named model **or**
swap it (Change…) for one you already have.

## Extended Thinking Mode

Ollama supports native reasoning for models trained for it (Qwen3.5, Gemma 4, DeepSeek-R1, QwQ, …). Reasoning is streamed into collapsible "Thinking" blocks in the chat.

Thinking is a property of the **agent**, not a global switch:

- For chat, pick a thinking agent in the chat panel: **Ollama Chat — Thinking** or **Ollama Chat — Gemma 4**
- For quick refactor, assign **Ollama Quick Refactor — Qwen3.5** or **— Gemma 4** in Agent Pipelines
- In your own agents, set `think = true` in the `[body]` table (top level, not under `[body.options]`)

Use a reasoning-capable model with these agents — a non-reasoning model simply ignores the flag.

