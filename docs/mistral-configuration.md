# Configure for Mistral AI

1. Open Qt Creator settings and navigate to QodeAssist > Providers
2. Select the bundled **Mistral AI** provider and enter your Mistral API key
   - For Codestral code completion, configure the separate **Codestral** provider — it uses its own key and endpoint (codestral.mistral.ai)
3. Go to QodeAssist > General > Agent Pipelines and assign Mistral agents to the features you want:
   - Code completion: **Mistral Completion — Codestral FIM** (needs the Codestral provider)
   - Chat assistant: **Mistral Chat** (or the Reasoning variant)
   - Chat compression: **Mistral Compression**
   - Quick refactor: **Mistral Quick Refactor**

To change the model or request parameters, duplicate the bundled agent on the
QodeAssist > Agents page (it extends the Mistral base agent) and edit your copy.
