# QodeAssist
[![Build plugin](https://github.com/Palm1r/QodeAssist/actions/workflows/build_cmake.yml/badge.svg?branch=main)](https://github.com/Palm1r/QodeAssist/actions/workflows/build_cmake.yml)

QodeAssist is an AI-powered coding assistant plugin for Qt Creator. It provides intelligent code completion and suggestions for C++ and QML, leveraging large language models through local providers like Ollama. Enhance your coding productivity with context-aware AI assistance directly in your Qt development environment.

## Supported LLM Providers
QodeAssist currently supports the following LLM (Large Language Model) providers:
- [Ollama](https://ollama.com)
- [LM Studio](https://lmstudio.ai)
- OpenAI compatible providers

## Supported Models
QodeAssist has been tested with the following language models, all trained for Fill-in-theMiddle:

Ollama:
- [starcoder2](https://ollama.com/library/starcoder2)
- [codellama](https://ollama.com/library/codellama)

LM studio:
- [second-state/StarCoder2-7B-GGUF](https://huggingface.co/second-state/StarCoder2-7B-GGUF)
- [TheBloke/CodeLlama-7B-GGUF](https://huggingface.co/TheBloke/CodeLlama-7B-GGUF)

Please note that while these models have been specifically tested and confirmed to work well with QodeAssist, other models compatible with the supported providers may also work. We encourage users to experiment with different models and report their experiences.
If you've successfully used a model that's not listed here, please let us know by opening an issue or submitting a pull request to update this list.

## Development Progress

- [x] Basic plugin with code autocomplete functionality
- [ ] Improve and automate settings
- [ ] Add chat functionality
- [ ] Support for more providers and models

## Installation Plugin

1. Install QtCreator 14.0
2. Install [Ollama](https://ollama.com). Make sure to review the system requirements before installation.
3. Install a language model in Ollama. For example, you can run:
```
ollama run starcoder2:7b
```
4. Download the QodeAssist plugin.
5. Launch Qt Creator and install the plugin:
   - Go to MacOS: Qt Creator -> About Plugins...
           Windows\Linux: Help -> About Plugins...
   - Click on "Install Plugin..."
   - Select the downloaded QodeAssist plugin archive file

## Configure Plugin

1. Open Qt Creator settings
2. Navigate to the "Qode Assist" tab
3. Choose your LLM provider (e.g., Ollama)
   - If you haven't added the provider to your system PATH, specify the path to the provider executable in the "Provider Paths" field
4. Select the installed model
   - If you need to enter the model name manually, it indicates that the plugin cannot locate the provider's executable file. However, this doesn't affect the plugin's functionality – it will still work correctly. This autoselector input option is provided for your convenience, allowing you to easily select and use different models
5. Choose the prompt template that corresponds to your model
6. Apply the settings

You're all set! QodeAssist is now ready to use in Qt Creator.

## Support the development of QodeAssist
If you find QodeAssist helpful, there are several ways you can support the project:

1. **Report Issues**: If you encounter any bugs or have suggestions for improvements, please [open an issue](https://github.com/Palm1r/qodeassist/issues) on our GitHub repository.

2. **Contribute**: Feel free to submit pull requests with bug fixes or new features.

3. **Spread the Word**: Star our GitHub repository and share QodeAssist with your fellow developers.

4. **Financial Support**: If you'd like to support the development financially, you can make a donation using one of the following cryptocurrency addresses:

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
