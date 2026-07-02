# Configure for OpenAI

QodeAssist supports both OpenAI's standard Chat Completions API and the Responses API, giving you access to the latest GPT models.

1. Open Qt Creator settings and navigate to QodeAssist > Providers
2. Select the bundled **OpenAI (Chat Completions)** or **OpenAI (Responses API)** provider and enter your OpenAI API key
3. Go to QodeAssist > General > Agent Pipelines and assign OpenAI agents to the features you want:
   - Code completion: **OpenAI Completion**
   - Chat assistant: **OpenAI Chat**, **OpenAI Chat — Mini** (Chat Completions), or **OpenAI Chat — Responses**
   - Chat compression: **OpenAI Compression**
   - Quick refactor: **OpenAI Quick Refactor**

To change the model or request parameters, duplicate the bundled agent on the
QodeAssist > Agents page (it extends the OpenAI base agent) and edit your copy.

Note for GPT-5 family models on Chat Completions: use `max_completion_tokens`
(not `max_tokens`) and `reasoning_effort` in the agent `[body]`; reasoning
models reject `temperature`.
