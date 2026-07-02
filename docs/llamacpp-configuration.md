# Configure for llama.cpp

1. Start `llama-server` locally (default http://localhost:8080)
2. Open Qt Creator settings and navigate to QodeAssist > Providers
   - Select the bundled **llama.cpp** provider and adjust the URL if your server runs elsewhere (no API key needed)
3. Go to QodeAssist > General > Agent Pipelines and assign llama.cpp agents to the features you want:
   - Code completion: **LlamaCpp Completion — FIM** (needs a FIM-capable model)
   - Chat assistant: **LlamaCpp Chat**
   - Chat compression: **LlamaCpp Compression**
   - Quick refactor: **LlamaCpp Quick Refactor**

To change the model or request parameters, duplicate the bundled agent on the
QodeAssist > Agents page (it extends the llama.cpp base agent) and edit your
copy. Disable `enable_tools` in your agent if the model doesn't support tool calling.
