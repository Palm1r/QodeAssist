# QodeAssist
[![Build plugin](https://github.com/Palm1r/QodeAssist/actions/workflows/build_cmake.yml/badge.svg?branch=main)](https://github.com/Palm1r/QodeAssist/actions/workflows/build_cmake.yml)

QodeAssist is an AI-powered coding assistant plugin for Qt Creator. It provides intelligent code completion and suggestions for C++ and QML, leveraging large language models through local providers like Ollama. Enhance your coding productivity with context-aware AI assistance directly in your Qt development environment.

<img src="https://github.com/user-attachments/assets/255a52f1-5cc0-4ca3-b05c-c4cf9cdbe25a" width="600" alt="QodeAssistPreview">

## Supported LLM Providers
QodeAssist currently supports the following LLM (Large Language Model) providers:
- [Ollama](https://ollama.com)
- [LM Studio](https://lmstudio.ai)
- OpenAI compatible providers

## Supported Models

QodeAssist has been thoroughly tested and optimized for use with the following language models, all of which are specifically trained for Fill-in-the-Middle (FIM) tasks:

- CodeLlama
- StarCoder2
- DeepSeek-Coder-V2-Lite-Base

These models have demonstrated excellent performance in code completion and assistance tasks within the QodeAssist environment.

### Custom Prompts

For advanced users or those with specific requirements, QodeAssist offers the flexibility to create, save, and load custom prompts using JSON templates. This feature allows you to tailor the AI's behavior to your exact needs.

To get started with custom prompts:

1. Navigate to the "Custom Template" option in the FIM Prompt Settings.
2. Create your custom JSON prompt template.
3. Use the "Save Custom Template to JSON" button to store your template for future use.
4. To use a previously saved template, click "Load Custom Template from JSON".
5. Make sure to select "Custom Template" from the dropdown menu in the FIM Prompt Settings on the General page to activate your custom template.

For inspiration and examples of effective custom prompts, please refer to the `rawPromptExamples` folder in our repository.

<img width="600" alt="Custom template" src="https://github.com/user-attachments/assets/4a14c552-baba-4531-ab4f-cb1f9ac6620b">
<img width="600" alt="Select custom template" src="https://github.com/user-attachments/assets/3651dafd-83f9-4df9-943f-69c28cd3d8a3">

### Tested Models

#### Ollama:
- [starcoder2](https://ollama.com/library/starcoder2)
- [codellama](https://ollama.com/library/codellama)

#### LM Studio:
- [second-state/StarCoder2-7B-GGUF](https://huggingface.co/second-state/StarCoder2-7B-GGUF)
- [TheBloke/CodeLlama-7B-GGUF](https://huggingface.co/TheBloke/CodeLlama-7B-GGUF)

Please note that while these models have been specifically tested and confirmed to work well with QodeAssist, other models compatible with the supported providers may also work. We encourage users to experiment with different models and report their experiences.

If you've successfully used a model that's not listed here, please let us know by opening an issue or submitting a pull request to update this list.

## Development Progress

- [x] Basic plugin with code autocomplete functionality
- [x] Improve and automate settings
- [ ] Add chat functionality
- [x] Sharing diff with model
- [ ] Sharing project source with model
- [ ] Support for more providers and models

## Plugin installation using Ollama as an example

1. Install QtCreator 14.0
2. Install [Ollama](https://ollama.com). Make sure to review the system requirements before installation.
3. Install a language models in Ollama. For example, you can run:

For suggestions:
```
ollama run codellama:7b-code
```
For chat:
```
ollama run codellama:7b-instruct
```
4. Download the QodeAssist plugin.
5. Launch Qt Creator and install the plugin:
   - Go to MacOS: Qt Creator -> About Plugins...
           Windows\Linux: Help -> About Plugins...
   - Click on "Install Plugin..."
   - Select the downloaded QodeAssist plugin archive file

## Configure Plugin

<img src="https://github.com/user-attachments/assets/0743d09e-1f02-44ed-9a1a-85e2a0a0c01a" width="800" alt="QodeAssist в действии">

1. Open Qt Creator settings
2. Navigate to the "Qode Assist" tab
3. Select "General" page
4. Choose your LLM provider (e.g., Ollama)
5. Select the installed model by the "Select Model" button
   - For LM Studio you will see current load model
6. Choose the prompt template that corresponds to your model
7. Apply the settings

You're all set! QodeAssist is now ready to use in Qt Creator.

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
