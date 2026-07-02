# QodeAssist — AI coding assistant for Qt Creator

[![Build plugin](https://github.com/Palm1r/QodeAssist/actions/workflows/build_cmake.yml/badge.svg?branch=main)](https://github.com/Palm1r/QodeAssist/actions/workflows/build_cmake.yml)
[![GitHub Tag](https://img.shields.io/github/v/tag/Palm1r/QodeAssist?label=release)](https://github.com/Palm1r/QodeAssist/releases)
![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/Palm1r/QodeAssist/total?color=41%2C173%2C71&label=downloads)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Discord](https://dcbadge.limes.pink/api/server/BGMkUsXUgf?style=flat)](https://discord.gg/BGMkUsXUgf)

![qodeassist-icon](https://github.com/user-attachments/assets/dc336712-83cb-440d-8761-8d0a31de898d) **QodeAssist** brings a full AI coding workflow to Qt Creator for C++ and QML — smart code completion, multi-panel chat, inline quick refactoring, and project-aware tool calling. It works with local runtimes (Ollama, llama.cpp, LM Studio) and cloud providers (Claude, OpenAI, Google AI, Mistral, Qwen, DeepSeek), can run as an **MCP server** so other clients reuse its project context, and can also act as an **MCP client** to consume tools from external MCP servers (authenticated MCP servers are not supported yet).

⚠️ **Important Notice About Paid Providers**
> When using paid providers like Claude, OpenRouter or OpenAI-compatible services:
> - These services will consume API tokens which may result in charges to your account
> - The QodeAssist developer bears no responsibility for any charges incurred
> - Please carefully review the provider's pricing and your account settings before use

## Table of Contents
1. [Overview](#overview)
2. [Install Plugin](#install-plugin-to-qtcreator)
3. [Configuration](#configuration)
4. [Features](#features)
5. [Context Layers](#context-layers)
6. [QtCreator Version Compatibility](#qtcreator-version-compatibility)
7. [Hotkeys](#hotkeys)
8. [Troubleshooting](#troubleshooting)
9. [Development Progress](#development-progress)
10. [Support the Development](#support-the-development-of-qodeassist)
11. [How to Build](#how-to-build)

## Overview

QodeAssist enhances Qt Creator with AI-powered coding assistance:

- **Code Completion** — intelligent, context-aware suggestions (FIM and chat models) for C++ and QML, with multiline support
- **Chat Assistant** — side panel, bottom panel, or detached window; history with auto-save, token monitoring, extended thinking
- **Quick Refactoring** — inline AI-assisted edits directly in the editor with a searchable custom-instructions library
- **Agent Tools** — read, search, create and edit files; build the project; run terminal commands; access linter/compiler issues; manage TODOs
- **Agent Skills** — reusable folders of specialized instructions loaded on demand; discovered from `.qodeassist/skills/` and `.claude/skills/`, invoked automatically, with `/skill`, or always-on
- **MCP Server** — expose QodeAssist's project-aware tools to external MCP clients (Claude Code, VS Code, Claude Desktop via bridge)
- **MCP Client Hub** — connect QodeAssist to external MCP servers and use their tools in Chat and Quick Refactor (authenticated MCP servers are not supported yet)
- **File Context** — attach, link, or auto-sync open editor files for richer prompts
- **Many Providers** — Ollama, llama.cpp, LM Studio (Chat + Responses), Claude, OpenAI (Chat + Responses), Google AI, Mistral, Codestral, OpenRouter, Qwen, DeepSeek, any OpenAI-compatible endpoint
- **Reasoning / Thinking** — streamed chain-of-thought is shown for reasoning models across Claude, Google, OpenAI Responses, and any OpenAI-compatible endpoint that returns `reasoning_content` (DeepSeek, Qwen QwQ/Qwen3-Thinking, LM Studio, OpenRouter, …)
- **Customizable** — per-agent personas (agent TOML `system_prompt`), reusable refactor templates, full prompt-template control

**Join our [Discord Community](https://discord.gg/BGMkUsXUgf)** to get support and connect with other users!

<details>
  <summary>Code completion: (click to expand)</summary>
  <img src="https://github.com/user-attachments/assets/255a52f1-5cc0-4ca3-b05c-c4cf9cdbe25a" width="600" alt="QodeAssistPreview">
</details>

<details>
  <summary>Quick refactor in code: (click to expand)</summary>
  <img src="https://github.com/user-attachments/assets/4a9092e0-429f-41eb-8723-cbb202fd0a8c" width="600" alt="QodeAssistPreview">
</details>

<details>
  <summary>Chat View Mode: (click to expand)</summary>
  <img src="https://github.com/user-attachments/assets/5914dd78-c8a4-4d35-889a-10ec493d4c4b" width="600" alt="QodeAssistChat2">
</details>

<details>
  <summary>Multiline Code completion: (click to expand)</summary>
  <img src="https://github.com/user-attachments/assets/c18dfbd2-8c54-4a7b-90d1-66e3bb51adb0" width="600" alt="QodeAssistPreview">
</details>

<details>
  <summary>Chat with LLM models in side panels: (click to expand)</summary>
  <img src="https://github.com/user-attachments/assets/ead5a5d9-b40a-4f17-af05-77fa2bcb3a61" width="600" alt="QodeAssistChat">
</details>

<details>
  <summary>Chat with LLM models in bottom panel: (click to expand)</summary>
  <img width="326" alt="QodeAssistBottomPanel" src="https://github.com/user-attachments/assets/4cc64c23-a294-4df8-9153-39ad6fdab34b">
</details>

<details>
  <summary>Chat in addtional window: (click to expand)</summary>
  <img width="851" height="865" alt="image" src="https://github.com/user-attachments/assets/a68894b7-886e-4501-a61b-7161ae34b427" />
</details>

<details>
  <summary>Automatic syncing with open editor files: (click to expand)</summary>
  <img width="600" alt="OpenedDocumentsSync" src="https://github.com/user-attachments/assets/08efda2f-dc4d-44c3-927c-e6a975090d2f">
</details>

<details>
  <summary>Example how tools works: (click to expand)</summary>
  <img width="600" alt="ToolsDemo" src="https://github.com/user-attachments/assets/cf6273ad-d5c8-47fc-81e6-23d929547f6c">
</details>

## Install plugin to QtCreator

### Method 1: Using the Extension Registry (Recommended)

You can install and update QodeAssist directly from within Qt Creator by adding the QodeAssist registry as an external extension repository.

1. Open the Extensions page (`Qt Creator → Extensions`) and switch to the **Browser** tab
2. Enable **Use External Repository**
3. Next to **Repository URLs**, click **Add** and paste the registry archive URL matching your Qt Creator version:
   - **Latest (up to date to QtCreator release)**: `https://github.com/Palm1r/extension-registry/archive/refs/heads/qodeassist.tar.gz`
   - **QtC 20**: `https://github.com/Palm1r/extension-registry/archive/refs/heads/qodeassist-qtc20.tar.gz`
   - **QtC 19**: `https://github.com/Palm1r/extension-registry/archive/refs/heads/qodeassist-qtc19.tar.gz`
   - **QtC 18**: `https://github.com/Palm1r/extension-registry/archive/refs/heads/qodeassist-qtc18.tar.gz`
<details>
  <summary>Example of extension registry: (click to expand)</summary>
  <img width="600" alt="RegistryExample" src="https://github.com/user-attachments/assets/8ab8cf10-72e7-4961-8c5a-21d530378a05">
</details>

4. Click **Apply** — QodeAssist will appear in the extensions list, where you can **Install** it
5. Updates can be installed from the same screen when a new version is published

> **Note:** This is an external repository not maintained by The Qt Company. By adding it you accept responsibility for managing the associated risks, as stated in the Extensions page.

### Method 2: Using QodeAssistUpdater (Beta)

QodeAssistUpdater is a command-line utility that automates plugin installation and updates with automatic Qt Creator version detection and checksum verification.

**Features:**
- Automatic Qt Creator version detection
- Install, update, or remove plugin with single command
- List all available plugin versions
- Install specific plugin version
- Checksum verification
- Non-interactive mode for CI/CD

**Installation:**

Download pre-built binary from [QodeAssistUpdater releases](https://github.com/Palm1r/QodeAssistUpdater/releases) or build from source

**Usage:**

```bash
# Check current status and available updates
./qodeassist-updater --status

# Install latest version
./qodeassist-updater --install
```

For more information, visit the [QodeAssistUpdater repository](https://github.com/Palm1r/QodeAssistUpdater).

### Method 3: Manual Installation

1. Install Latest Qt Creator
2. Download the QodeAssist plugin for your Qt Creator
   - Remove old version plugin if already was installed
      - on macOS for QtCreator 16: ~/Library/Application Support/QtProject/Qt Creator/plugins/16.0.0/petrmironychev.qodeassist
      - on windows for QtCreator 16: C:\Users\<user>\AppData\Local\QtProject\qtcreator\plugins\16.0.0\petrmironychev.qodeassist\lib\qtcreator\plugins  
3. Launch Qt Creator and install the plugin:
   - Go to: 
     - MacOS: Qt Creator -> About Plugins...
     - Windows\Linux: Help -> About Plugins...
   - Click on "Install Plugin..."
   - Select the downloaded QodeAssist plugin archive file

## Configuration

### Quick Setup

QodeAssist is configured through three settings pages (Preferences > QodeAssist):

1. **Providers** — pick a bundled provider instance (Claude, OpenAI, Google AI, Mistral, Ollama, …) and enter its API key. Local providers (Ollama, llama.cpp, LM Studio) need no key.
2. **Agents** — every feature runs an *agent*: a bundled TOML preset combining a provider, model, and request template. The bundled agents work out of the box; create your own by extending a base agent under a new name.
3. **General > Agent Pipelines** — assign agents to the four feature slots: code completion (ordered, routed per file), chat assistant (picker allow-list), chat compression, and quick refactor. Local Ollama agents are pre-assigned by default; an unassigned slot disables that feature.

### Providers

Choose your preferred provider:

**Local providers:**
- **[Ollama](docs/ollama-configuration.md)** — native Ollama API
- **Ollama (OpenAI-compatible)** — Ollama's `/v1` endpoint for tool-calling models
- **[llama.cpp](docs/llamacpp-configuration.md)** — local `llama-server`
- **LM Studio** — OpenAI-compatible Chat API
- **LM Studio (Responses API)** — newer models that require the Responses endpoint

**Cloud providers:**
- **[Anthropic Claude](docs/claude-configuration.md)** — manual setup guide
- **[OpenAI](docs/openai-configuration.md)** — Chat Completions and Responses API
- **[Mistral AI](docs/mistral-configuration.md)** / **Codestral**
- **[Google AI](docs/google-ai-configuration.md)** — Gemini
- **Qwen (Alibaba)** — DashScope OpenAI-compatible endpoint (`qwen-plus`, `qwen-max`, `qwen-coder`)
- **DeepSeek** — OpenAI-compatible endpoint, `deepseek-chat` and `deepseek-reasoner`
- **OpenAI-compatible** — OpenRouter and any custom endpoint

### Recommended Models for Best Experience

For optimal coding assistance, we recommend using these top-tier models:

**Best for Code Completion & Refactoring:**
- **Claude 4.5 Haiku or Sonnet** (Anthropic)
- **GPT-5.1-codex or codex-mini** (OpenAI Responses API)
- **Codestral** (Mistral)

**Best for Chat Assistant:**
- **Claude 4.5 Sonnet** (Anthropic) - Outstanding reasoning and natural conversation flow
- **GPT-5.1-codex** (OpenAI Responses API) - Latest model with advanced capabilities
- **Gemini 2.5 or 3.0** (Google AI) - Latest models from Google
- **Mistral large** (Mistral) - Fast and capable

**Local models:**
- **Qwen3-coder** (Qwen) - Best local models

### Additional Configuration

- **[Creating and Extending Agents](docs/creating-agents.md)** - Add custom agents (including personas via `system_prompt`) with TOML profiles
- **[Chat Summarization](docs/chat-summarization.md)** - Compress conversations to save context tokens
- **[Ignoring Files](docs/ignoring-files.md)** - Exclude files from context using `.qodeassistignore`

## Features

### Code Completion
- AI-powered intelligent code completion
- Support for C++ and QML
- Context-aware suggestions
- Multiline completions

#### Completion Trigger Modes

QodeAssist offers two trigger modes for code completion:

**Hint-based**
- Shows a hint indicator near cursor when you type 3+ characters
- Press **Space** (or custom key) to request completion
- **Best for**: Paid APIs (Claude, OpenAI), conscious control
- **Benefits**: No unexpected API charges, full control over requests, no workflow interruption
- **Visual**: Clear indicator shows when completion is ready

**Automatic(Default)**
- Automatically requests completion after typing threshold
- Works immediately without additional keypresses
- **Best for**: Local models (Ollama, llama.cpp), maximum automation
- **Benefits**: Hands-free experience, instant suggestions

💡 **Tip**: Start with Hint-based to avoid unexpected costs. Switch to Automatic if using free local models.

Configure in: `Tools → Options → QodeAssist → Code Completion → General Settings`

### Chat Assistant
- Multiple chat panels: side panel, bottom panel, and popup window
- Chat history with auto-save and restore
- Token usage monitoring
- AI personas via agent `system_prompt` — switch personas by switching agents (see [Creating Agents](docs/creating-agents.md))
- **[Chat Summarization](docs/chat-summarization.md)** - Compress long conversations into AI-generated summaries
- **[File Context](docs/file-context.md)** - Attach or link files for better context
- Automatic syncing with open editor files (optional)
- Extended thinking / reasoning mode - shows streamed chain-of-thought for reasoning models (Claude, Google, OpenAI Responses, and OpenAI-compatible endpoints returning `reasoning_content` such as DeepSeek, Qwen, LM Studio, OpenRouter)

### Quick Refactoring
- Inline code refactoring directly in the editor with AI assistance
- **Custom instructions library** with search and autocomplete
- Create, edit, and manage reusable refactoring templates
- Combine base instructions with specific details
- **[Learn more](docs/quick-refactoring.md)**

### Tools & Function Calling

Chat and Quick Refactor can call tools to inspect and modify your project. Each tool can be individually enabled/disabled in settings.

| Tool | What it does |
|------|--------------|
| `list_project_files` | List files in the active project(s) |
| `find_file` | Find a file by name or partial path |
| `read_file` | Read file contents (project or absolute path) |
| `search_project` | Grep / symbol search across project sources |
| `create_new_file` | Create a new empty file on disk |
| `edit_file` | Replace content in a file (old → new) |
| `build_project` | Build the active project and return compiler output |
| `get_issues_list` | Read current linter / compiler diagnostics |
| `execute_terminal_command` | Run a shell command (with confirmation) |
| `todo_tool` | Track multi-step task progress during a conversation |

### Skills

**Agent Skills** package specialized instructions and workflows into reusable folders the AI loads on demand. QodeAssist implements the open [Agent Skills](https://agentskills.io) format, so skills authored for Claude Code, Cursor, or other agents work as-is.

A skill is a folder containing a `SKILL.md` file — YAML frontmatter (`name`, `description`) plus Markdown instructions:

```
my-skill/
└── SKILL.md
```

```markdown
---
name: my-skill
description: What the skill does and when to use it.
---

# My Skill

Step-by-step instructions for the task...
```

**Where skills are discovered:**
- **Project skills** — project-relative subdirectories (default `.qodeassist/skills/` and `.claude/skills/`), configured in `Projects → QodeAssist → Skills`. Project skills win over global ones on a name collision.
- **Global skills** — absolute directories shared across all projects (default includes `~/.claude/skills/`), configured in `Tools → Options → QodeAssist → Skills`.

Both settings pages show the list of currently discovered skills.

**How skills are used in Chat:**
- **Automatically** — each skill's name and description is added to the system prompt; when a request matches, the model loads the full instructions via the `load_skill` tool (requires a tool-calling model).
- **Explicitly** — type `/` in the chat input and pick a skill from the popup; its instructions are injected into that one message. Works with any model.
- **Always-on** — a skill whose frontmatter has `metadata: always-on: "true"` is injected into every chat request automatically.

Enable or disable the whole feature in `Tools → Options → QodeAssist → Skills`.

### MCP Server

QodeAssist can run an **MCP (Model Context Protocol) server** on `localhost`, exposing the tools above to external clients — so you can use QodeAssist's project awareness from Claude Code CLI, VS Code, Cursor, Claude Desktop, or any other MCP-capable client.

- **Enable** in `Tools → Options → QodeAssist → MCP Server`
- **Transport**: HTTP + SSE by default; a stdio bridge is provided for clients that only speak stdio (e.g. Claude Desktop)
- **Ready-to-copy snippets** for Claude Code, VS Code, and the bridge are available via the "Show connection instructions" button in settings

### MCP Client Hub

QodeAssist can also act as an **MCP client**, connecting to external MCP servers and making their tools available to Chat and Quick Refactor alongside the built-in ones.

- **Configure** servers in `Tools → Options → QodeAssist → MCP Client`
- **Transports**: stdio and HTTP/SSE
- **Limitation**: authenticated MCP servers (OAuth / token-protected) are **not supported yet** — only servers that accept unauthenticated local connections work for now

## Context Layers

QodeAssist uses a flexible prompt composition system that adapts to different contexts. Here's how prompts are constructed for each feature:

<details>
  <summary><strong>Code Completion (FIM Models)</strong> - Codestral, Qwen2.5-Coder, DeepSeek-Coder (click to expand)</summary>

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CODE COMPLETION (FIM Models)                        │
├─────────────────────────────────────────────────────────────────────────────┤
│  Examples: Codestral, Qwen2.5-Coder, DeepSeek-Coder                        │
│                                                                              │
│  1. Editor Context:                                                         │
│     ├─ File information (language, path)                                   │
│     ├─ Recent project changes (optional, if enabled)                       │
│     └─ Open editor files (optional, if enabled)                            │
│  2. Code Context:                                                           │
│     ├─ Code before cursor (prefix)                                         │
│     └─ Code after cursor (suffix)                                          │
│                                                                              │
│  Final Request: the agent's TOML [body] template renders prefix/suffix     │
│                 into the provider's native FIM fields                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

</details>

<details>
  <summary><strong>Code Completion (Non-FIM Models)</strong> - DeepSeek-Coder-Instruct, Qwen2.5-Coder-Instruct (click to expand)</summary>

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     CODE COMPLETION (Non-FIM/Chat Models)                   │
├─────────────────────────────────────────────────────────────────────────────┤
│  Examples: DeepSeek-Coder-Instruct, Qwen2.5-Coder-Instruct                 │
│                                                                              │
│  1. Completion Instructions (from the agent's TOML profile)                │
│     └─ Includes response formatting rules                                  │
│  2. Editor Context:                                                         │
│     ├─ File information (language, path)                                   │
│     ├─ Recent project changes (optional, if enabled)                       │
│     └─ Open editor files (optional, if enabled)                            │
│  3. Code Context:                                                           │
│     ├─ Code before cursor                                                  │
│     ├─ <cursor> marker                                                     │
│     └─ Code after cursor                                                   │
│                                                                              │
│  Final Prompt: [System: Instructions + EditorContext]                      │
│                [User: Code around cursor as a completion request]           │
└─────────────────────────────────────────────────────────────────────────────┘
```

</details>

<details>
  <summary><strong>Chat Assistant</strong> - Interactive coding assistant (click to expand)</summary>

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            CHAT ASSISTANT                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│  1. Agent System Prompt (persona, from the agent's TOML profile)           │
│  2. Project Info + Skills (catalog and always-on skills)                   │
│  3. Tool Definitions (if the agent enables tools)                          │
│  4. Conversation History:                                                   │
│     ├─ Previous messages and tool calls/results                            │
│     ├─ Attachments stay with the message they were sent with               │
│     └─ /skill instructions persist for the whole conversation              │
│  5. Linked Files + Open Editor Files (if auto-sync enabled):               │
│     └─ FRESH snapshot of current file content, re-read on every            │
│        request and placed next to your latest message — never              │
│        duplicated into the history                                          │
│  6. User Message (+ this turn's attachments and images)                    │
│                                                                              │
│  Final Prompt: [System: Persona + ProjectInfo + Skills]                    │
│                [History: Previous messages]                                 │
│                [User: CurrentFilesSnapshot + UserMessage + Attachments]     │
└─────────────────────────────────────────────────────────────────────────────┘
```

</details>

<details>
  <summary><strong>Quick Refactoring</strong> - Inline code improvements (click to expand)</summary>

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         QUICK REFACTORING                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│  1. Agent System Prompt (persona, from the agent's TOML profile)           │
│  2. Code Context (generated):                                               │
│     ├─ File information (language, path)                                   │
│     ├─ Code before selection (configurable amount)                         │
│     ├─ <selection_start> marker                                            │
│     ├─ Selected code (or current line)                                     │
│     ├─ <selection_end> marker                                              │
│     ├─ <cursor> marker (position within selection)                         │
│     ├─ Code after selection (configurable amount)                          │
│     └─ Output formatting and indentation rules                             │
│  3. Refactor Instruction (the user message):                               │
│     ├─ Built-in (e.g., "Improve Code", "Alternative Solution")             │
│     ├─ Custom Instruction (from library)                                   │
│     │  └─ ~/.config/QtProject/qtcreator/qodeassist/                        │
│     │      quick_refactor/instructions/*.json                              │
│     └─ Additional Details (optional user input)                            │
│  4. Tool Definitions (if the agent enables tools)                          │
│                                                                              │
│  Final Prompt: [System: Persona + CodeContext + Rules]                     │
│                [User: Instruction + Details]                                │
└─────────────────────────────────────────────────────────────────────────────┘
```

</details>

### Key Points

- **System Prompts** live in the agent's TOML profile (`system_prompt`); switch personas by switching agents
- **Linked and open-synced files are always current**: their content is not stored in the conversation — every request re-reads the files and sends a fresh snapshot next to your latest message. Editing a linked file between messages never leaves a stale copy in the context, and changing it does not invalidate the provider's prompt cache for the whole conversation
- **One-time attachments are different**: they are saved with the message they were sent with and stay in the history as sent
- **FIM vs Non-FIM** for code completion is the agent's choice: a FIM agent renders prefix/suffix into native FIM fields, an instruct agent sends a chat-shaped request — pick the agent that matches your model
- **Quick Refactor** has its own agent roster, independent from Chat
- **Custom Instructions** provide reusable templates that can be augmented with specific details
- **Tool Calling** is available for Chat and Quick Refactor when the agent enables it; tool rounds per request are limited (configurable in `Settings → Tools`)

See the [Quick Refactoring Guide](docs/quick-refactoring.md) for more details.

## QtCreator Version Compatibility

| Qt Creator Version | QodeAssist Version |
|-------------------|-------------------|
| 17.0.0+ | 0.6.0 - 0.x.x |
| 16.0.2 | 0.5.13 - 0.9.6 |
| 16.0.1 | 0.5.7 - 0.5.13 |
| 16.0.0 | 0.5.2 - 0.5.6 |
| 15.0.1 | 0.4.8 - 0.5.1 |
| 15.0.0 | 0.4.0 - 0.4.7 |
| 14.0.2 | 0.2.3 - 0.3.x |
| 14.0.1 | ≤ 0.2.2 |

## Hotkeys

All hotkeys can be customized in Qt Creator Settings. Default hotkeys:

| Action | macOS | Windows/Linux |
|--------|-------|--------------|
| Open chat window | ⌥⌘W | Ctrl+Alt+W |
| Close chat window | ⌥⌘S | Ctrl+Alt+S |
| Manual code suggestion | ⌥⌘Q | Ctrl+Alt+Q |
| Accept full suggestion | Tab | Tab |
| Accept word | ⌥→ | Alt+→ |
| Quick refactor | ⌥⌘R | Ctrl+Alt+R |
    
## Troubleshooting

Having issues with QodeAssist? Check our [detailed troubleshooting guide](docs/troubleshooting.md) for:

- Connection issues and provider URLs
- Model and template compatibility
- Platform-specific issues (Linux, macOS, Windows)
- Resetting settings to defaults
- Common problems and solutions

For additional support, join our [Discord Community](https://discord.gg/BGMkUsXUgf) or check [GitHub Issues](https://github.com/Palm1r/QodeAssist/issues).

## Development Progress

- [x] Code completion (FIM and chat models)
- [x] Chat assistant (side / bottom / detached panels)
- [x] Quick refactoring with custom-instructions library
- [x] Diff sharing with models
- [x] Tools / function calling (file I/O, build, terminal, diagnostics)
- [x] Agent Skills (project + global directories, `/skill` commands, always-on, `load_skill` tool)
- [x] MCP (Model Context Protocol) — QodeAssist as a server
- [x] MCP — QodeAssist as a client (consume external MCP tools; authenticated MCP servers not yet supported)
- [ ] Full project source sharing
- [ ] Additional provider support

## Support the development of QodeAssist
If you find QodeAssist helpful, there are several ways you can support the project:

1. **Report Issues**: If you encounter any bugs or have suggestions for improvements, please [open an issue](https://github.com/Palm1r/qodeassist/issues) on our GitHub repository.

2. **Contribute**: Feel free to submit pull requests with bug fixes or new features. The easiest contribution is an agent preset for a provider or model you use — it's a single TOML file, no C++ required; see [Contributing your agent](docs/creating-agents.md#contributing-your-agent-to-qodeassist).

3. **Spread the Word**: Star our GitHub repository and share QodeAssist with your fellow developers.

4. **Financial Support**: If you'd like to support the development financially, you can make a donation using one of the following:
   - Paypal: [my paypalme page](https://www.paypal.com/paypalme/palm1r)
   - Bitcoin (BTC): `bc1qndq7f0mpnlya48vk7kugvyqj5w89xrg4wzg68t`
   - Ethereum (ETH): `0xA5e8c37c94b24e25F9f1f292a01AF55F03099D8D`
   - Litecoin (LTC): `ltc1qlrxnk30s2pcjchzx4qrxvdjt5gzuervy5mv0vy`
   - USDT (TRC20): `THdZrE7d6epW6ry98GA3MLXRjha1DjKtUx`

Every contribution, no matter how small, is greatly appreciated and helps keep the project alive!

## Related Projects

- **[LLMQore](https://github.com/Palm1r/llmqore)** — the standalone LLM-core library extracted from QodeAssist, reusable in other Qt/C++ projects
- **[QodeAssistUpdater](https://github.com/Palm1r/QodeAssistUpdater)** — CLI installer/updater for the plugin

## How to Build

### Prerequisites
- CMake 3.16+
- C++20 compatible compiler
- Qt Creator development files

### Build Steps

1. Create a build directory:

```bash
mkdir build && cd build
```

2. Configure and build:

```bash
cmake -DCMAKE_PREFIX_PATH=<path_to_qtcreator> -DCMAKE_BUILD_TYPE=RelWithDebInfo <path_to_plugin_source>
cmake --build .
```

**Path specifications:**
- `<path_to_qtcreator>`:
  - **Windows/Linux**: Qt Creator build directory or combined binary package
  - **macOS**: `Qt Creator.app/Contents/Resources/`
- `<path_to_plugin_source>`: Path to this plugin directory

## For Contributors

### Adding an agent preset

New provider/model presets are plain TOML — extend a provider base, register the file in `agents.qrc`, and the test suite validates it automatically. Step-by-step guide: [docs/creating-agents.md](docs/creating-agents.md#contributing-your-agent-to-qodeassist).

### Code Style

- **QML**: Follow [QML Coding Guide](https://github.com/Furkanzmc/QML-Coding-Guide) by @Furkanzmc
- **C++**: Use `.clang-format` configuration in the project root
- Run formatting before submitting PRs

### Development Guidelines

For detailed development guidelines and architecture patterns, see [docs/architecture.md](docs/architecture.md) and [docs/target-architecture.md](docs/target-architecture.md).

## License

QodeAssist is licensed under the **GNU General Public License v3.0**
(see [`LICENSE`](LICENSE)), with **additional attribution terms under
GPLv3 Section 7(b)**.

You are free to use, modify, and redistribute QodeAssist under GPL-3.0,
but you **must preserve** the original author attribution, copyright
notices, and project identification — including in source file headers,
the plugin metadata (`QodeAssist.json.in`), and the About dialog or
equivalent user-facing identification. Modified versions must be clearly
marked as different from the original.

### Commercial licensing

QodeAssist is also available under a separate commercial license for use
in proprietary or closed-source products without GPL-3.0 obligations.
For commercial licensing inquiries, contact **palm1r-github-dev@pm.me**.

### Qt Creator components and attributions

QodeAssist is a plugin for Qt Creator and incorporates certain components
(plugin templates, API headers, and related boilerplate) originating from
Qt Creator, which are copyright (C) The Qt Company Ltd.

These components are provided by The Qt Company under the GNU General
Public License version 3, annotated with **The Qt Company GPL Exception
1.0**. This exception permits the development and distribution of Qt
Creator plugins under licenses of the plugin author's own choosing,
notwithstanding the GPL's general linking requirements. It is this
exception that allows QodeAssist to be offered under both GPL-3.0 and a
separate commercial license.

The original copyright and license notices of The Qt Company are
preserved in the relevant source files and must not be removed.

For Qt Creator's licensing terms, see
[LICENSE.GPL3-EXCEPT](https://github.com/qt-creator/qt-creator/blob/master/LICENSES/LICENSE.GPL3-EXCEPT).

![qodeassist-icon](https://github.com/user-attachments/assets/dc336712-83cb-440d-8761-8d0a31de898d)
![qodeassist-icon-small](https://github.com/user-attachments/assets/8ec241bf-3186-452e-b8db-8d70543c2f41)

