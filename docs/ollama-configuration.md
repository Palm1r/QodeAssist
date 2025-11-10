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
4. Navigate to the "QodeAssist" tab
5. On the "General" page, verify:
    - Ollama is selected as your LLM provider
    - The URL is set to http://localhost:11434
    - Your installed model appears in the model selection
    - The prompt template is Ollama Auto FIM or Ollama Auto Chat for chat assistance. You can specify template if it is not work correct
    - Disable using tools if your model doesn't support tooling
6. Click Apply if you made any changes

You're all set! QodeAssist is now ready to use in Qt Creator.

<details>
  <summary>Example of Ollama settings: (click to expand)</summary>

<img width="824" alt="Ollama Settings" src="https://github.com/user-attachments/assets/ed64e03a-a923-467a-aa44-4f790e315b53" />
</details>

