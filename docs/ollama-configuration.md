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

## Extended Thinking Mode

Ollama supports extended thinking mode for models that are capable of deep reasoning (such as DeepSeek-R1, QwQ, and similar reasoning models). This mode allows the model to show its step-by-step reasoning process before providing the final answer.

### How to Enable

**For Chat Assistant:**
1. Navigate to Qt Creator > Preferences > QodeAssist > Chat Assistant
2. In the "Extended Thinking (Claude, Ollama)" section, check "Enable extended thinking mode"
3. Select a reasoning-capable model (e.g., deepseek-r1:8b, qwq:32b)
4. Click Apply

**For Quick Refactoring:**
1. Navigate to Qt Creator > Preferences > QodeAssist > Quick Refactor
2. Check "Enable Thinking Mode"
3. Configure thinking budget and max tokens as needed
4. Click Apply

### Supported Models

Thinking mode works best with models specifically designed for reasoning:
- **DeepSeek-R1** series (deepseek-r1:8b, deepseek-r1:14b, deepseek-r1:32b)
- **QwQ** series (qwq:32b)
- Other models trained for chain-of-thought reasoning

### How It Works

When thinking mode is enabled:
1. The model generates internal reasoning (visible in the chat as "Thinking" blocks)
2. After reasoning, it provides the final answer
3. You can collapse/expand thinking blocks to focus on the final answer
4. Temperature is automatically set to 1.0 for optimal reasoning performance

**Technical Details:**
- Thinking mode adds the `enable_thinking: true` parameter to requests sent to Ollama
- This is natively supported by the Ollama API for compatible models
- Works in both Chat Assistant and Quick Refactoring contexts

<details>
  <summary>Example of Ollama settings: (click to expand)</summary>

<img width="824" alt="Ollama Settings" src="https://github.com/user-attachments/assets/ed64e03a-a923-467a-aa44-4f790e315b53" />
</details>

