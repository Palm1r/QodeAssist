# QodeAssist - AI-powered coding assistant plugin for Qt Creator
[![Discord](https://dcbadge.limes.pink/api/server/https://discord.gg/DGgMtTteAD?style=flat?theme=discord)](https://discord.gg/DGgMtTteAD)
[![Build plugin](https://github.com/Palm1r/QodeAssist/actions/workflows/build_cmake.yml/badge.svg?branch=main)](https://github.com/Palm1r/QodeAssist/actions/workflows/build_cmake.yml)
![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/Palm1r/QodeAssist/total?color=41%2C173%2C71)
![GitHub Tag](https://img.shields.io/github/v/tag/Palm1r/QodeAssist)
![Static Badge](https://img.shields.io/badge/QtCreator-14.0.2-brightgreen)

QodeAssist is an AI-powered coding assistant plugin for Qt Creator. It provides intelligent code completion and suggestions for C++ and QML, leveraging large language models through local providers like Ollama. Enhance your coding productivity with context-aware AI assistance directly in your Qt development environment.

## Table of Contents
1. [Overview](#overview)
2. [Installation](#installation)
3. [Configure Plugin](#configure-plugin)
4. [Supported LLM Providers](#supported-llm-providers)
5. [Recommended Models](#recommended-models)
   - [Ollama](#ollama)
   - [LM Studio](#lm-studio)
6. [QtCreator Version Compatibility](#qtcreator-version-compatibility)
7. [Development Progress](#development-progress)
8. [Hotkeys](#hotkeys)
9. [Troubleshooting](#troubleshooting)
10. [Support the Development](#support-the-development-of-qodeassist)
11. [How to Build](#how-to-build)

## Overview

- AI-powered code completion
- Chat functionality:
  - Side and Bottom panels
- Support for multiple LLM providers:
  - Ollama
  - LM Studio
  - OpenAI-compatible local providers
- Extensive library of model-specific templates
- Custom template support
- Easy configuration and model selection

<details>
  <summary>Code completion: (click to expand)</summary>
  <img src="https://github.com/user-attachments/assets/255a52f1-5cc0-4ca3-b05c-c4cf9cdbe25a" width="600" alt="QodeAssistPreview">
</details>

<details>
  <summary>Chat with LLM models in side panels: (click to expand)</summary>
  <img src="https://github.com/user-attachments/assets/ead5a5d9-b40a-4f17-af05-77fa2bcb3a61" width="600" alt="QodeAssistChat">
</details>

<details>
  <summary>Chat with LLM models in bottom panel: (click to expand)</summary>
  <img width="326" alt="QodeAssistBottomPanel" src="https://github.com/user-attachments/assets/4cc64c23-a294-4df8-9153-39ad6fdab34b">
</details>

## Installation

1. Install Latest QtCreator
2. Install [Ollama](https://ollama.com). Make sure to review the system requirements before installation.
3. Install a language models in Ollama via terminal. For example, you can run:

For suggestions:
```
ollama run codellama:7b-code
```
For chat:
```
ollama run codellama:7b-instruct
```
4. Download the QodeAssist plugin for your QtCreator.
5. Launch Qt Creator and install the plugin:
   - Go to MacOS: Qt Creator -> About Plugins...
           Windows\Linux: Help -> About Plugins...
   - Click on "Install Plugin..."
   - Select the downloaded QodeAssist plugin archive file

## Configure Plugin

<details>
  <summary>Configure plugins: (click to expand)</summary>
  <img src="https://github.com/user-attachments/assets/00ad980f-b470-48eb-9aaa-077783d38798" width="600" alt="Configuere QodeAssist">
</details>

1. Open Qt Creator settings
2. Navigate to the "Qode Assist" tab
3. Select "General" page
4. Choose your LLM provider (e.g., Ollama)
5. Select the installed model by the "Select Model" button
   - For LM Studio you will see current loaded model
6. Choose the prompt template that corresponds to your model
7. Apply the settings

You're all set! QodeAssist is now ready to use in Qt Creator.

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/P5P412V96G)

## Supported LLM Providers
QodeAssist currently supports the following LLM (Large Language Model) providers:
- [Ollama](https://ollama.com)
- [LM Studio](https://lmstudio.ai)
- OpenAI compatible providers

## Recommended Models:
QodeAssist has been thoroughly tested and optimized for use with the following language models:

- Llama
- CodeLlama
- StarCoder2
- DeepSeek-Coder-V2
- Qwen-2.5

### Ollama:
### For autocomplete(FIM)
```
ollama run codellama:7b-code
ollama run starcoder2:7b
ollama run qwen2.5-coder:7b-base
ollama run deepseek-coder-v2:16b-lite-base-q3_K_M
```
### For chat
```
ollama run codellama:7b-instruct
ollama run starcoder2:instruct
ollama run qwen2.5-coder:7b-instruct
ollama run deepseek-coder-v2
```

### LM Studio:
   similar models, like for ollama

Please note that while these models have been specifically tested and confirmed to work well with QodeAssist, other models compatible with the supported providers may also work. We encourage users to experiment with different models and report their experiences.

If you've successfully used a model that's not listed here, please let us know by opening an issue or submitting a pull request to update this list.

## QtCreator Version Compatibility

- QtCreator 14.0.2 - 0.2.3 - 0.3.x 
- QtCreator 14.0.1 - 0.2.2 plugin version and below

## Development Progress

- [x] Basic plugin with code autocomplete functionality
- [x] Improve and automate settings
- [x] Add chat functionality
- [x] Sharing diff with model
- [ ] Sharing project source with model
- [ ] Support for more providers and models

## Hotkeys

- To call manual request to suggestion, you can use or change it in settings
    - on Mac: Option + Command + Q
    - on Windows: Ctrl + Alt + Q
- To insert the full suggestion, you can use the TAB key
- To insert line by line, you can use the "Move cursor word right" shortcut:
    - On Mac: Option + Right Arrow
    - On Windows: Alt + Right Arrow
    
## Troubleshooting

If QodeAssist is having problems connecting to the LLM provider, please check the following:

1. Verify the IP address and port:

    - For Ollama, the default is usually http://localhost:11434
    - For LM Studio, the default is usually http://localhost:1234

2. Check the endpoint:

Make sure the endpoint in the settings matches the one required by your provider
    - For Ollama, it should be /api/generate
    - For LM Studio and OpenAI compatible providers, it's usually /v1/chat/completions

3. Confirm that the selected model and template are compatible:

Ensure you've chosen the correct model in the "Select Models" option
Verify that the selected prompt template matches the model you're using

If you're still experiencing issues with QodeAssist, you can try resetting the settings to their default values:
1. Open Qt Creator settings
2. Navigate to the "Qode Assist" tab
3. Pick settings page for reset
4. Click on the "Reset Page to Defaults" button
    - The API key will not reset
    - Select model after reset

## Support the development of QodeAssist
If you find QodeAssist helpful, there are several ways you can support the project:

1. **Report Issues**: If you encounter any bugs or have suggestions for improvements, please [open an issue](https://github.com/Palm1r/qodeassist/issues) on our GitHub repository.

2. **Contribute**: Feel free to submit pull requests with bug fixes or new features.

3. **Spread the Word**: Star our GitHub repository and share QodeAssist with your fellow developers.

4. **Financial Support**: If you'd like to support the development financially, you can make a donation using one of the following:
   - [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/P5P412V96G)
   - Bitcoin (BTC): `bc1qndq7f0mpnlya48vk7kugvyqj5w89xrg4wzg68t`
   - Ethereum (ETH): `0xA5e8c37c94b24e25F9f1f292a01AF55F03099D8D`
   - Litecoin (LTC): `ltc1qlrxnk30s2pcjchzx4qrxvdjt5gzuervy5mv0vy`
   - USDT (TRC20): `THdZrE7d6epW6ry98GA3MLXRjha1DjKtUx`

Every contribution, no matter how small, is greatly appreciated and helps keep the project alive!

## How to Build

Create a build directory and run

    cmake -DCMAKE_PREFIX_PATH=<path_to_qtcreator> -DCMAKE_BUILD_TYPE=RelWithDebInfo <path_to_plugin_source>
    cmake --build .

where `<path_to_qtcreator>` is the relative or absolute path to a Qt Creator build directory, or to a
combined binary and development package (Windows / Linux), or to the `Qt Creator.app/Contents/Resources/`
directory of a combined binary and development package (macOS), and `<path_to_plugin_source>` is the
relative or absolute path to this plugin directory.

## For Contributors

QML code style: Preferably follow the following guidelines https://github.com/Furkanzmc/QML-Coding-Guide, thank you @Furkanzmc for collect them
C++ code style: check use .clang-fortmat in project
