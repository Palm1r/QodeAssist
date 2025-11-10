# Troubleshooting

## Connection Issues

### 1. Verify provider URL and port

Make sure you're using the correct default URLs:
- **Ollama**: `http://localhost:11434`
- **LM Studio**: `http://localhost:1234`
- **llama.cpp**: `http://localhost:8080`

### 2. Check model and template compatibility

- Ensure the correct model is selected in settings
- Verify that the selected prompt template matches your model
- Some models may not support certain features (e.g., tool calling)

### 3. Linux compatibility

- Prebuilt binaries support Ubuntu 22.04+
- For other distributions, you may need to [build manually](../README.md#how-to-build)
- See [issue #48](https://github.com/Palm1r/QodeAssist/issues/48) for known Linux compatibility issues and solutions

## Reset Settings

If issues persist, you can reset settings to their default values:

1. Open Qt Creator → Settings → QodeAssist
2. Select the settings page you want to reset
3. Click "Reset Page to Defaults" button

**Note:**
- API keys are preserved during reset
- You will need to re-select your model after reset

## Common Issues

### Plugin doesn't appear after installation

1. Restart Qt Creator completely
2. Check that the plugin is enabled in About Plugins
3. Verify you downloaded the correct version for your Qt Creator

### No suggestions appearing

1. Check that code completion is enabled in QodeAssist settings
2. Verify your provider/model is running and accessible
3. Check Qt Creator's Application Output pane for error messages
4. Try manual suggestion hotkey (⌥⌘Q on macOS, Ctrl+Alt+Q on Windows/Linux)

### Chat not responding

1. Verify your API key is configured correctly (for cloud providers)
2. Check internet connection (for cloud providers)
3. Ensure the provider service is running (for local providers)
4. Look for error messages in the chat panel

## Getting Help

If you continue to experience issues:

1. Check existing [GitHub Issues](https://github.com/Palm1r/QodeAssist/issues)
2. Join our [Discord Community](https://discord.gg/BGMkUsXUgf) for support
3. Open a new issue with:
   - Qt Creator version
   - QodeAssist version
   - Operating system
   - Provider and model being used
   - Steps to reproduce the problem

