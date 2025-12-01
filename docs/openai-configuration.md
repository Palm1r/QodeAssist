# Configure for OpenAI

QodeAssist supports both OpenAI's standard Chat Completions API and the new Responses API, giving you access to the latest GPT models including GPT-5.1 and GPT-5.1-codex.

## Standard OpenAI Configuration

1. Open Qt Creator settings and navigate to the QodeAssist section
2. Go to Provider Settings tab and configure OpenAI api key
3. Return to General tab and configure:
   - Set "OpenAI" as the provider for code completion or/and chat assistant
   - Set the OpenAI URL (https://api.openai.com)
   - Select your preferred model (e.g., gpt-4o, gpt-5.1, gpt-5.1-codex)
   - Choose the OpenAI template for code completion or/and chat

<details>
  <summary>Example of OpenAI settings: (click to expand)</summary>

<img width="829" alt="OpenAI Settings" src="https://github.com/user-attachments/assets/4716f790-6159-44d0-a8f4-565ccb6eb713" />
</details>

## OpenAI Responses API Configuration

The Responses API is OpenAI's newer endpoint that provides enhanced capabilities and improved performance. It supports the latest GPT-5.1 models.

1. Open Qt Creator settings and navigate to the QodeAssist section
2. Go to Provider Settings tab and configure OpenAI api key
3. Return to General tab and configure:
   - Set "OpenAI Responses" as the provider for code completion or/and chat assistant
   - Set the OpenAI URL (https://api.openai.com)
   - Select your preferred model (e.g., gpt-5.1, gpt-5.1-codex)
   - Choose the OpenAI Responses template for code completion or/and chat

