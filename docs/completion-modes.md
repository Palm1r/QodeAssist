# Code completion modes

QodeAssist produces an inline code-completion suggestion (ghost text you accept with Tab)
in one of four ways, chosen per setup in **Preferences → QodeAssist → Code Completion →
Completion mode**. All four share the same trigger heuristics, ghost-text rendering, and
Tab / word / line accept — they differ only in *how* the suggestion is produced. Pick the
one that matches your model's capability.

| Mode | What it does | Best for |
|---|---|---|
| **Classic FIM** | One fill-in-the-middle request to the configured provider, using the code around the cursor. | Weak or older local models; the lowest-latency option; the default. |
| **FIM + context** | The same FIM request, enriched with semantic context read from Qt Creator's built-in code model (C++ class outline and referenced declarations; QML sibling components). Chat-type templates only. | Capable models where extra context improves the completion and the added prompt size is affordable. |
| **Agentic (local tools)** | A tool-capable provider model runs the in-process tool loop, gathers its own context, and returns the completion by calling the `propose_completion` tool. | Strong tool-calling models, on demand. Multi-round and slow — **manual trigger only**. |
| **Agentic (ACP agent)** | An external ACP agent (chosen below the mode selector) does the same over the QodeAssist MCP server. | Using an external coding agent as the completion engine. Manual trigger only; needs an agent selected. |

## Notes and requirements

- **Default and migration.** Existing setups open as **Classic FIM** with unchanged
  behavior — the mode is a new setting with a behavior-preserving default, and your
  connection, model, template, and API-key settings are untouched.
- **FIM + context** only enriches C++ and QML files, and only when the active template is a
  chat-type template. The built-in code model can lag unsaved edits, so the added context
  is a hint, not a guarantee; the request still runs for unsupported languages, just without
  the extra context. (Qt Creator's built-in `CPlusPlus` model is used, not clangd's AST,
  which is not reachable by plugins.)
- **Agentic modes are manual-trigger only.** Because a tool loop takes seconds and many
  tokens, the agentic modes are *not* fired automatically while you type — invoke them with
  the **Request QodeAssist Suggestion** shortcut (default `Ctrl+Alt+Q`, reconfigurable in
  Preferences → Keyboard). They need a tool-capable provider (local) and a chat-type
  template.
- **Agentic (ACP agent)** additionally needs an agent id from the Agents catalogue entered
  in **ACP completion agent id**, and the QodeAssist MCP server running. With no agent
  selected, the mode is disabled — it never falls back to a FIM request; a manual trigger
  reports the missing configuration instead.

See also: [file-context.md](file-context.md) for how the plain-text context window is
assembled, and [acp-agents.md](acp-agents.md) for configuring ACP agents.
