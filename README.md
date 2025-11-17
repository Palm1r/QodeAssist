# QodeAssist - AI-powered coding assistant plugin for Qt Creator
[![Build plugin](https://github.com/Palm1r/QodeAssist/actions/workflows/build_cmake.yml/badge.svg?branch=main)](https://github.com/Palm1r/QodeAssist/actions/workflows/build_cmake.yml)
![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/Palm1r/QodeAssist/total?color=41%2C173%2C71)
![GitHub Tag](https://img.shields.io/github/v/tag/Palm1r/QodeAssist)
[![](https://dcbadge.limes.pink/api/server/BGMkUsXUgf?style=flat)](https://discord.gg/BGMkUsXUgf)

![qodeassist-icon](https://github.com/user-attachments/assets/dc336712-83cb-440d-8761-8d0a31de898d) QodeAssist is an AI-powered coding assistant plugin for Qt Creator. It provides intelligent code completion and suggestions for C++ and QML, leveraging large language models through local providers like Ollama. Enhance your coding productivity with context-aware AI assistance directly in your Qt development environment.

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
5. [QtCreator Version Compatibility](#qtcreator-version-compatibility)
6. [Hotkeys](#hotkeys)
7. [Troubleshooting](#troubleshooting)
8. [Development Progress](#development-progress)
9. [Support the Development](#support-the-development-of-qodeassist)
10. [How to Build](#how-to-build)

## Overview

QodeAssist enhances Qt Creator with AI-powered coding assistance:

- **Code Completion**: Intelligent, context-aware code suggestions for C++ and QML
- **Chat Assistant**: Multiple interface options (popup window, side panel, bottom panel)
- **Quick Refactoring**: Inline AI-assisted code improvements directly in editor with custom instructions library
- **File Context**: Attach or link files for better AI understanding
- **Tool Calling**: AI can read project files, search code, and access diagnostics
- **Multiple Providers**: Support for Ollama, Claude, OpenAI, Google AI, Mistral AI, llama.cpp, and more
- **Customizable**: Project-specific rules, custom instructions, and extensive model templates

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

### Method 1: Using QodeAssistUpdater (Beta)

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

### Method 2: Manual Installation

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

QodeAssist supports multiple LLM providers. Choose your preferred provider and follow the configuration guide:

### Supported Providers

- **[Ollama](docs/ollama-configuration.md)** - Local LLM provider
- **[llama.cpp](docs/llamacpp-configuration.md)** - Local LLM server
- **[Anthropic Claude](docs/claude-configuration.md)** - Сloud provider
- **[OpenAI](docs/openai-configuration.md)** - Сloud provider
- **[Mistral AI](docs/mistral-configuration.md)** - Сloud provider
- **[Google AI](docs/google-ai-configuration.md)** - Сloud provider
- **LM Studio** - Local LLM provider
- **OpenAI-compatible** - Custom providers (OpenRouter, etc.)

### Additional Configuration

- **[Project Rules](docs/project-rules.md)** - Customize AI behavior for your project
- **[Ignoring Files](docs/ignoring-files.md)** - Exclude files from context using `.qodeassistignore`

## Features

### Code Completion
- AI-powered intelligent code completion
- Support for C++ and QML
- Context-aware suggestions
- Multiline completions

### Chat Assistant
- Multiple chat panels: side panel, bottom panel, and popup window
- Chat history with auto-save and restore
- Token usage monitoring
- **[File Context](docs/file-context.md)** - Attach or link files for better context
- Automatic syncing with open editor files (optional)
- Extended thinking mode (Claude, other providers in plan) - Enable deeper reasoning for complex tasks

### Quick Refactoring
- Inline code refactoring directly in the editor with AI assistance
- Selection-based improvements with instant code replacement
- Built-in quick actions (repeat, improve, alternative)
- **Custom instructions library** with search and autocomplete
- Create, edit, and manage reusable refactoring templates
- Combine base instructions with specific details
- **[Learn more](docs/quick-refactoring.md)**

### Tools & Function Calling
- Read project files
- List and search in project
- Access linter/compiler issues
- Enabled by default (can be disabled)

## QtCreator Version Compatibility

| Qt Creator Version | QodeAssist Version |
|-------------------|-------------------|
| 17.0.0+ | 0.6.0 - 0.x.x |
| 16.0.2 | 0.5.13 - 0.x.x |
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

- [x] Code completion functionality
- [x] Chat assistant with multiple panels
- [x] Diff sharing with models
- [x] Tools/function calling support
- [x] Project-specific rules
- [ ] Full project source sharing
- [ ] Additional provider support
- [ ] MCP (Model Context Protocol) support

## Support the development of QodeAssist
If you find QodeAssist helpful, there are several ways you can support the project:

1. **Report Issues**: If you encounter any bugs or have suggestions for improvements, please [open an issue](https://github.com/Palm1r/qodeassist/issues) on our GitHub repository.

2. **Contribute**: Feel free to submit pull requests with bug fixes or new features.

3. **Spread the Word**: Star our GitHub repository and share QodeAssist with your fellow developers.

4. **Financial Support**: If you'd like to support the development financially, you can make a donation using one of the following:
   - Bitcoin (BTC): `bc1qndq7f0mpnlya48vk7kugvyqj5w89xrg4wzg68t`
   - Ethereum (ETH): `0xA5e8c37c94b24e25F9f1f292a01AF55F03099D8D`
   - Litecoin (LTC): `ltc1qlrxnk30s2pcjchzx4qrxvdjt5gzuervy5mv0vy`
   - USDT (TRC20): `THdZrE7d6epW6ry98GA3MLXRjha1DjKtUx`

Every contribution, no matter how small, is greatly appreciated and helps keep the project alive!

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

### Code Style

- **QML**: Follow [QML Coding Guide](https://github.com/Furkanzmc/QML-Coding-Guide) by @Furkanzmc
- **C++**: Use `.clang-format` configuration in the project root
- Run formatting before submitting PRs

### Development Guidelines

For detailed development guidelines, architecture patterns, and best practices, see the [project workspace rules](.cursor/rules.mdc).

![qodeassist-icon](https://github.com/user-attachments/assets/dc336712-83cb-440d-8761-8d0a31de898d)
![qodeassist-icon-small](https://github.com/user-attachments/assets/8ec241bf-3186-452e-b8db-8d70543c2f41)

