# QodeAssist Architecture

This document describes the runtime architecture of QodeAssist after the
migration of all LLM runtime paths onto the agent / `Session` stack
("Stack B"). Every runtime LLM path — code completion, chat (send/stream +
compression + token counting), and quick refactor — now goes through agents,
`Session`, and the `Providers::GenericProvider` layer.

> Legend: ✅ = on Stack B (active runtime), 🔴 = legacy Stack A (isolated, no
> runtime consumers left).

---

## 1. Top level: ownership and dependency injection

The plugin (`qodeassist.cpp`) owns everything via `new` + parent (no plugin-wide
singletons; each feature receives its dependencies explicitly).

```
QodeAssistPlugin
  Stack B infrastructure:
    • Providers::registerBuiltinProviders()   — registers 13 client_api types
    • ProviderInstanceFactory                 — 14 instances from TOML
    • ProviderSecretsStore
    • AgentFactory                            — agents from TOML
    • SessionManager(agentFactory)
    • m_engine (QQmlEngine)
        rootContext: "agentFactory", "sessionManager"   — DI for chat (QML)

  Wired into consumers:
    • QodeAssistClient ← LLMClientInterface(*sessionManager, *agentFactory)
                       ← setSessionManager / setAgentFactory   (for quick refactor)
```

Chat lives in QML (`ChatRootView` is a `QML_ELEMENT`), so `AgentFactory` and
`SessionManager` are exposed as **context properties on the engine's root
context** and resolved in `ChatRootView` via
`qmlEngine(this)->rootContext()->contextProperty(...)`.

---

## 2. Stack B core (agent / Session)

```
AgentFactory.create(name)
  configByName(name) → AgentConfig (TOML)
     providerInstance, model, endpoint, role, messageFormat,
     sampling, enableTools, enableThinking, match{filePatterns,...}
  buildProviderForAgent:
     instance = ProviderInstanceFactory.instanceByName(cfg.providerInstance)
     provider = ProviderFactory::create(instance.clientApi)        ◄── keystone
     provider.setUrl(instance.url)
     provider.setApiKey(secrets.read(instance.apiKeyRef))
  ▼
Agent(config, provider)
  promptTemplate = JsonPromptTemplate::fromConfig(cfg.messageFormat)   (inja)
  ▼
SessionManager.createSession(agentName) → Session(agent)
  ├─ ConversationHistory     — messages as ContentBlocks
  ├─ SystemPromptBuilder     — layers: agent.role + caller layers
  └─ ResponseRouter(client)  — emits ResponseEvent

Session API:
  • send(blocks, toolsOverride)   — chat/refactor: append user msg + dispatch
  • sendCompletion(ContextData)   — completion: FIM prefix/suffix
  • client()                      — agent's LLMQore::BaseClient (direct streaming)
  • systemPrompt()->setLayer(...) — dynamic context layers
  • supportsImages()              — provider Image capability
  • history()                     — for seeding from ChatModel
```

`Session::sendCompletion` and `dispatch` compose `SystemPromptBuilder` layers
(`agent.role` + caller-provided) into the request system prompt.

---

## 3. Provider layer — the keystone (implemented during migration)

The Stack B provider layer previously existed only as an abstract base +
empty factory (`registerType` was never called, no concrete providers). This
blocked every agent from obtaining a working provider. It is now implemented
via a single configuration-driven `GenericProvider`.

```
ProviderFactory  (sources/providers, namespace functions)
   registerType(name, fn) / create(name, parent) / knownNames()
        ▲
        │ registerBuiltinProviders()   — client_api → provider table
        │
GenericProvider : Providers::Provider
   • owns an LLMQore::BaseClient (created by a ClientFactory)
   • prepareRequest — inherited from Provider base:
        delegates to PromptTemplate::buildFullRequest
   • client() / providerID() / capabilities() / getInstalledModels()
```

### client_api → provider table

| client_api                     | LLMQore client          | ProviderID       | capabilities            |
|--------------------------------|-------------------------|------------------|-------------------------|
| Claude                         | ClaudeClient            | Claude           | Tools·Thinking·Image·ModelListing |
| Google AI                      | GoogleAIClient          | GoogleAI         | Tools·Thinking·Image·ModelListing |
| llama.cpp                      | LlamaCppClient          | LlamaCpp         | Tools·Thinking·Image·ModelListing |
| Mistral AI                     | MistralClient           | MistralAI        | Tools·Thinking·Image·ModelListing |
| Codestral                      | MistralClient           | MistralAI        | Tools·Image             |
| Ollama (Native)                | OllamaClient            | Ollama           | Tools·Thinking·Image·ModelListing |
| Ollama (OpenAI-compatible)     | OpenAIClient            | OpenAICompatible | Tools·Thinking·Image·ModelListing |
| OpenAI (Chat Completions)      | OpenAIClient            | OpenAI           | Tools·Thinking·Image·ModelListing |
| OpenAI (Responses API)         | OpenAIResponsesClient   | OpenAIResponses  | Tools·Thinking·Image·ModelListing |
| OpenAI Compatible              | OpenAIClient            | OpenAICompatible | Tools·Image·Thinking    |
| OpenRouter                     | OpenAIClient            | OpenRouter       | Tools·Image·Thinking·ModelListing |
| LM Studio (Chat Completions)   | OpenAIClient            | LMStudio         | Tools·Thinking·Image·ModelListing |
| LM Studio (Responses API)      | OpenAIResponsesClient   | OpenAIResponses  | Tools·Thinking·Image·ModelListing |

Request *shape* comes from the agent's prompt template (jinja `messageFormat`),
so a single provider class covers every API by varying only the client factory
and metadata.

---

## 4. Runtime paths (all on Stack B)

### 4a. Code completion ✅

```
Qt Creator LSP (getCompletionsCycling)
  ▼
LLMClientInterface
  pickCompletionAgent: AgentRouter.pickAgent(roster.codeCompletion, {file, project})
  session = sessionManager.createSession(agent)
  ctx = Templates::ContextData{ prefix, suffix,
                                systemPrompt = fileContext + openFiles }
  session.sendCompletion(ctx)
     ▼ stream from session.client():
  requestCompleted → sendCompletionToClient → CodeHandler → LSP
  system prompt = agent.role; FIM template renders prefix/suffix
```

### 4b. Chat ✅

```
ChatRootView (QML)
  resolve agentFactory()/sessionManager() = qmlEngine(this)->rootContext()
  ChatAgentController: agent list (configNames), active agent (persisted),
                       supportsThinking/Tools
  QML agent picker (TopBar.agentSelector) — replaced provider/model/template combos
  ▼ dispatchSend
ClientInterface
  session = sessionManager.createSession(currentChatAgent)
  registerQodeAssistTools(session.client().tools()) + registerSkillTool
  systemPrompt layer "chat.context" = project info + skills + linked files
  seedHistory(session.history() ← ChatModel: user/assistant/tool-call+result)
  session.send(userBlocks{text + images}, useTools)
     ▼ stream from session.client() → existing handlers → ChatModel:
  chunk→addMessage  thinking→addThinkingBlock
  tool→addToolExecutionStatus / updateToolResult
  finalized→usage   completed→messageReceivedCompletely → removeSession

ChatCompressor    → createSession(agent) → seed history → layer "compression" → send(prompt)
InputTokenCounter → estimate without provider (calibrated by server usage)
```

### 4c. Quick refactor ✅

```
QodeAssistClient.requestQuickRefactor → QuickRefactorHandler (setSessionManager/setAgentFactory)
  pickRefactorAgent: AgentRouter.pickAgent(roster.quickRefactor, {file, project})
  session = createSession(agent)
  if useTools: registerQodeAssistTools(session.client().tools())
  systemPrompt layer "refactor" = buildSystemPrompt(tagged content +
                                  output requirements + indentation rules)
  session.send(blocks{instructions}, useTools)
     ▼ stream from session.client():
  requestCompleted → ResponseCleaner → RefactorResult → insert into editor
```

---

## 5. Configuration sources

```
~/.config/.../qodeassist/config/
  providers/*.toml   → ProviderInstance { name, client_api, url, api_key_ref }
  agents/*.toml      → AgentConfig { providerInstance, model, endpoint, role,
                                     messageFormat, sampling, match, enable* }
  pipelines rosters  → codeCompletion / chatAssistant / chatCompression / quickRefactor
                       consumed by AgentRouter.pickAgent(roster, {filePath, projectName})

Editor policy (NOT agent config):
  CodeCompletionSettings — triggers, modelOutputHandler, context extraction,
                           useOpenFilesContext
                           (sampling / prompt-generation fields removed)
```

---

## 6. Remaining Stack A (runtime does NOT depend on it)

```
🔴 Settings UI: provider/model/template selection pages
                (ccProvider / caProvider / qrProvider) + ConfigurationManager
                → use ProvidersManager
🔴 root providers/*  (PluginLLMCore::Provider, 14 classes)
                → read only chat/quick-refactor sampling settings
🔴 pluginllmcore/*   (ProvidersManager, PromptTemplateManager, ResponseCleaner,
                      PromptProviderChat/Fim, ContextData)
🔴 qodeassist.cpp:144-146  registerProviders() / registerTemplates()  (Stack A registration)
🔴 qodeassist.cpp:185      MCP skill-tool loop on Stack A providers  (effectively dead)
🔴 ChatAssistantSettings / QuickRefactorSettings — sampling fields (read only by root providers)

ResponseCleaner (pluginllmcore) is still used by QuickRefactorHandler as a text
utility — orthogonal to the provider stack.
```

### Removed during the migration

- Rules subsystem (`RulesLoader` + chat "active rules" UI + QuickRefactor rules block)
- `ChatConfigurationController`, `AgentRoleController` (chat config/role presets)
- `m_promptProvider` (`PromptProviderFim`) in the plugin
- `RequestType::CodeCompletion` branch in all 14 root providers
- Sampling / prompt-generation fields in `CodeCompletionSettings`
- ChatView no longer links `PluginLLMCore`

---

## 7. Dependency summary

```
                 ┌──────────────── Stack B (active runtime) ────────────────┐
LLMClientInterface ─┐                                                        │
ClientInterface ────┼─► SessionManager ─► Session ─► Agent ─► GenericProvider ─► LLMQore::*Client
QuickRefactorHandler─┘        │              │         │            │
ChatCompressor ──────────────┘              │      AgentFactory  ProviderFactory
                                  AgentRouter (rosters)  │            │
                                                ProviderInstanceFactory (TOML)
                 └──────────────────────────────────────────────────────────┘

   Stack A (settings UI + ConfigurationManager + MCP loop) — isolated,
   no runtime consumers remain.
```

---

## 8. Open follow-ups (optional)

1. **Chat picker filtering** — show only `chatAssistant`-roster agents (currently
   lists all non-hidden agents; the auto-default may land on a FIM agent).
   Requires wiring ChatView to `PipelinesConfig` (watch for OBJECT-library
   symbol duplication).
2. **MCP tools on agent clients** — MCP skill tools are registered only on Stack A
   providers; to expose MCP tools to chat agents, register them on the session
   client alongside `registerQodeAssistTools`.
3. **Physical Stack A teardown** — remove the provider/model/template settings UI,
   `ConfigurationManager`, root `providers/*`, `pluginllmcore/*`, and the
   registration + MCP loop in `qodeassist.cpp`. Runtime no longer depends on them.
4. **Per-message session cost** — chat/refactor create a fresh agent/provider/client
   (and read secrets) per request; a session pool could reduce latency.
```
