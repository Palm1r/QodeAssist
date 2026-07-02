# Configure for Google AI

1. Open Qt Creator settings and navigate to QodeAssist > Providers
2. Select the bundled **Google AI** provider and enter your Google AI Studio API key
3. Go to QodeAssist > General > Agent Pipelines and assign Google agents to the features you want:
   - Code completion: **Google Completion**
   - Chat assistant: **Google Chat**
   - Chat compression: **Google Compression**
   - Quick refactor: **Google Quick Refactor**

To change the Gemini model or request parameters, duplicate the bundled agent on
the QodeAssist > Agents page (it extends the Google base agent) and edit your copy.
