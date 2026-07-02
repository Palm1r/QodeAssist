# Configure for Anthropic Claude

1. Open Qt Creator settings and navigate to QodeAssist > Providers
2. Select the bundled **Claude** provider and enter your Anthropic API key
3. Go to QodeAssist > General > Agent Pipelines and assign Claude agents to the features you want:
   - Code completion: **Claude Completion**
   - Chat assistant: **Claude Chat — Sonnet** (or an Opus variant)
   - Chat compression: **Claude Compression**
   - Quick refactor: **Claude Quick Refactor** (or the Fast variant)

To change the model or request parameters, duplicate the bundled agent on the
QodeAssist > Agents page (it extends the Claude base agent) and edit your copy.
