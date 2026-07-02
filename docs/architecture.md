# QodeAssist Architecture

This document describes the **current** runtime architecture, after the §10
rework in `target-architecture.md` was completed. Every runtime LLM path —
code completion, chat (send/stream + compression + token counting), and quick
refactor — flows through one stack: agents, `Session`, and the
`Providers::GenericProvider` layer. There is no legacy parallel path; the old
"Stack A" (root `providers/*`, `pluginllmcore/*`, `ConfigurationManager`, the
provider/model/template settings pages) has been removed.

For the design rationale, layering contract, and cross-cutting policies, see
[`target-architecture.md`](target-architecture.md). This file documents how the
code is wired today.

---

## 1. Top level: ownership and dependency injection

The plugin (`qodeassist.cpp`) owns everything via `new` + parent — no
plugin-wide singletons; each feature receives its dependencies explicitly.

```
QodeAssistPlugin
    • Providers::registerBuiltinProviders()   — client_api → provider table
    • ProviderInstanceFactory                 — provider instances from TOML
    • ProviderSecretsStore                    — secrets behind a port
    • AgentFactory                            — agents from TOML + agent_models.json
    • SessionManager(agentFactory)            — owns the ToolContributorRegistry
        toolContributors().add(registerQodeAssistTools)
        toolContributors().add(registerSkillTool)
        toolContributors().add(McpClientsManager::registerToolsOn)
    • m_engine (QQmlEngine)
        rootContext: "agentFactory", "sessionManager"   — DI for chat (QML)

  Wired into consumers:
    • QodeAssistClient ← LLMClientInterface(generalSettings, completeSettings,
                            agentFactory, sessionManager, documentReader,
                            performanceLogger)
                       ← setSessionManager / setAgentFactory   (quick refactor)
```

Chat lives in QML (`ChatRootView` is a `QML_ELEMENT`), so `AgentFactory` and
`SessionManager` are exposed as **context properties on the engine's root
context** and resolved in `ChatRootView` via
`qmlEngine(this)->rootContext()->contextProperty(...)`.

---

## 2. Core (agent / Session)

```
AgentFactory.create(name)
  configByName(name) → AgentConfig (TOML, [body] table; model override from
                       agent_models.json applied here)
  buildProviderForAgent:
     instance = ProviderInstanceFactory.instanceByName(cfg.providerInstance)
     provider = ProviderFactory::create(instance.clientApi)
     provider.setUrl(instance.url)
     provider.setApiKey(secrets.read(instance.apiKeyRef))
  ▼
Agent(config, provider)
  promptTemplate = JsonPromptTemplate::fromConfig(cfg)   — compiles [body] (inja),
                   validated at load against a synthetic context
  provider.setPromptCaching(cfg.cachePrompt, cfg.cacheTtl == "1h")
  ▼
SessionManager — three ways to obtain a Session:
  • createDetachedSession(externalHistory)      — chat: one persistent, caller-owned
                                                  session over an externally-owned
                                                  history; the agent is (re)bound via
                                                  rebindAgentByName when the picker
                                                  changes (rebinding cancels any
                                                  in-flight request)
  • createSession(agentName, externalHistory?)  — manager-tracked session, removed
                                                  via removeSession
  • acquire(agentName) / release(session)       — one-shot pipelines: a small
                                                  per-agent pool of internal-history
                                                  sessions; acquire hands out a
                                                  session with cleared history,
                                                  cleared system-prompt layers,
                                                  cleared client tools and cleared
                                                  pinned context / content loader /
                                                  bindings; release refuses to pool
                                                  external-history sessions
  ▼
Session(agent[, externalHistory])
  ├─ ConversationHistory     — messages as polymorphic ContentBlocks
  ├─ SystemPromptBuilder     — ordered named layers (priority-sorted)
  └─ ResponseRouter(client)  — adapts client signals → typed ResponseEvent

Session API:
  • send(blocks)                  — the ONLY dispatch entry point: append a user
                                    message and dispatch. Completion/chat/refactor
                                    differ only in block content + template; tools
                                    on/off comes from the agent's enable_tools.
  • cancel()                      — tears down in-flight; emits cancelled(id)
  • history() / systemPrompt() / client()
  • setContentLoader(loader)      — resolves Stored* attachment/image blocks
  • lastError() → ErrorInfo       — typed synchronous start-failure detail

Session signals (three-state, mutually exclusive per request):
  • finished(id, stopReason)
  • failed(id, ErrorInfo{category, message, providerDetail})
  • cancelled(id)
  + event(ResponseEvent)          — live delta stream for the chat UI
```

`Session::dispatch` renders the agent's `system_prompt` into the `agent.system`
layer, composes all `SystemPromptBuilder` layers into the request system prompt,
and substitutes `${MODEL}` in the endpoint before sending.

---

## 3. Provider layer

One configuration-driven `GenericProvider` covers every API; it varies only by
the LLMQore client factory and metadata. Request *shape* belongs to the agent's
`JsonPromptTemplate` (the `[body]` table), never to the provider.

```
ProviderFactory  (sources/providers, namespace functions)
   registerType(name, fn) / create(name, parent) / knownNames()
        ▲  registerBuiltinProviders()   — client_api → provider table
GenericProvider : Providers::Provider
   • owns an LLMQore::BaseClient (created by a ClientFactory)
   • prepareRequest → PromptTemplate::buildFullRequest; injects tools when
     enable_tools; applies ClaudeCacheControl when prompt caching is on
   • client() / providerID() / getInstalledModels()
```

### client_api → provider table

| client_api                   | LLMQore client        | ProviderID       |
|------------------------------|-----------------------|------------------|
| Claude                       | ClaudeClient          | Claude           |
| Google AI                    | GoogleAIClient        | GoogleAI         |
| llama.cpp                    | LlamaCppClient        | LlamaCpp         |
| Mistral AI                   | MistralClient         | MistralAI        |
| Codestral                    | MistralClient         | MistralAI        |
| Ollama (Native)              | OllamaClient          | Ollama           |
| Ollama (OpenAI-compatible)   | OpenAIClient          | OpenAICompatible |
| OpenAI (Chat Completions)    | OpenAIClient          | OpenAI           |
| OpenAI (Responses API)       | OpenAIResponsesClient | OpenAIResponses  |
| OpenAI Compatible            | OpenAIClient          | OpenAICompatible |
| OpenRouter                   | OpenAIClient          | OpenRouter       |
| LM Studio (Chat Completions) | OpenAIClient          | LMStudio         |
| LM Studio (Responses API)    | OpenAIResponsesClient | OpenAIResponses  |

---

## 4. Configuration model

```
~/.config/.../qodeassist/config/
  providers/*.toml   → ProviderInstance { name, client_api, url, api_key_ref }
  agents/*.toml      → AgentConfig { schema_version, providerInstance, model,
                                     endpoint, system_prompt, [body], match,
                                     enable_tools, enable_thinking, cache_prompt,
                                     extends, abstract, hidden, tags }
  agent_models.json  → per-agent model override (applied by AgentFactory)
  pipelines          → codeCompletion (ordered roster, routed by AgentRouter.pickAgent
                       on {filePath, projectName}); chatAssistant (allow-list for the
                       chat picker); chatCompression / quickRefactor (single agent each)

Editor policy (NOT agent config):
  CodeCompletionSettings — triggers, modelOutputHandler, context extraction,
                           useOpenFilesContext
```

`[body]` **is** the request body (deep-mergeable through `extends`; Jinja-bearing
string values render and splice as raw JSON, literals pass through, empty renders
drop the key). `include` resolves only sandboxed partial roots. Profiles validate
at load: a referenced partial must resolve and the assembled body must parse as
JSON against a synthetic context — config errors surface in the agents settings
page, never as a silent runtime drop. The loader also lints: unknown top-level /
`[match]` keys and same-layer duplicate names are warnings; a user file that
reuses a bundled agent's name is rejected (bundled agents cannot be replaced —
users extend them, or the per-provider abstract bases, under a new name);
`abstract` and `hidden` are never inherited through `extends`. Full spec:
[`agent-templates-design.md`](agent-templates-design.md); user-facing guide:
[`creating-agents.md`](creating-agents.md).

---

## 5. Runtime paths

Agent selection depends on the pipeline. Code completion is the only
context-routed one: `AgentRouter.pickAgent(roster.codeCompletion, {file,
project})` walks the ordered roster and returns the first agent whose `[match]`
fits. Chat filters to the `chatAssistant` allow-list and the user picks; quick
refactor and compression each use a single configured agent.

### 5a. Code completion

```
Qt Creator LSP (getCompletionsCycling)
  ▼
LLMClientInterface
  agent   = AgentRouter.pickAgent(roster.codeCompletion, {file, project})
  session = sessionManager.acquire(agent)                 — pooled
  systemPrompt layer "completion.context" = fileContext + open-files context
  session.send( blocks{ CompletionContent(prefix, suffix) } )
     ▼ on Session::finished:
  history().lastAssistantText() → CodeHandler (output-mode) → LSP items
     → sessionManager.release(session)
```

The completion context travels as a `CompletionContent` block; the template
exposes it as `ctx.prefix` / `ctx.suffix`. FIM vs instruct is purely agent
config (the body), not feature code. Completion never touches the delta stream —
it waits for `finished` and reads the last message.

### 5b. Chat

`ChatRootView` owns one persistent `ConversationHistory` for the whole chat view
and injects it into every collaborator. **History is the single source of truth.**

```
ChatRootView (QML)  — owns ConversationHistory m_history
  ChatModel.setHistory(m_history)          — ChatModel is a PROJECTION:
        subscribes to messageAdded/Updated/cleared/reset, flattens blocks→rows,
        overlays file-edit status from ChangesManager, holds a per-message usage map
  ChatAgentController                       — picker filtered to the
        chatAssistant allow-list; active agent persisted
  ▼ dispatchSend
ClientInterface
  session = sessionManager.createDetachedSession(m_history)   — one persistent
            session per chat view; ensureAgentBound → rebindAgentByName on
            picker change
  sessionManager.toolContributors().contribute(client.tools())   — builtin+skills+MCP
  session.setContentLoader(ChatSerializer::loadContentFromStorage)
  systemPrompt layer "chat.context" = project info + skills + linked files
  session.send( blocks{ TextContent + StoredAttachmentContent + StoredImageContent } )
     ▼ consumes Session signals (NOT raw client signals):
  event(Usage)        → ChatModel.setMessageUsage + token-counter calibration
  finished(id)        → ChangesManager.applyPendingEditsForRequest + persist
  failed(id, ErrorInfo) → surface error
  cancelled(id)       → clear busy state (the persistent session and history survive
                        every outcome)

ChatCompressor    → acquire(chatCompression agent — single configured) → send the
                    flattened transcript as one user message → read summary from the
                    compression session's own history → write a fresh v0.3 file →
                    release
InputTokenCounter → estimates over ConversationHistory (calibrated by Usage events)
ChatSerializer    → persists ConversationHistory via MessageSerializer (v0.3);
                    imports legacy v0.1/v0.2 files
```

`ChatModel`'s QML role surface (roleType / content / attachments / images /
isRedacted / token roles) is unchanged, so the QML delegates were untouched. The
projection's incremental updates avoid model resets on the streaming hot path.

### 5c. Quick refactor

```
QodeAssistClient.requestQuickRefactor → QuickRefactorHandler
  agent   = pipelines.quickRefactor (single configured agent)
  session = sessionManager.acquire(agent)
  if useTools: sessionManager.toolContributors().contribute(client.tools())
  systemPrompt layer "refactor" = tagged selection + output + indentation rules
  session.send(blocks{instructions})
     ▼ on Session::finished:
  history().lastAssistantText() → ResponseCleaner → RefactorResult → editor insert
     → sessionManager.release(session)
  on Session::failed(ErrorInfo) → RefactorResult{error}
```

---

## 6. Context layer

The context services sit behind IDE-agnostic ports; Qt Creator API use lives in
the adapters.

```
EditorContext   — IDocumentReader (port)  ← DocumentReaderQtCreator (TextEditor API)
ProjectContext  — IProjectScanner (port)  ← ProjectScannerQtCreator (ProjectExplorer
                  + Core::DocumentModel + the IgnoreManager for .qodeassistignore)
TokenEstimator  — TokenUtils (pure)       ← InputTokenCounter (thin UI consumer)
```

`ContextManager` is now Qt-Creator-free: it delegates open-file enumeration and
ignore filtering to an injected `IProjectScanner` (defaulting to the QtC adapter),
and keeps only filesystem reads + formatting. `ContextManager::shouldIgnore(path)`
replaced the previously exposed `ignoreManager()`.

---

## 7. Cross-cutting

- **Request lifecycle** — a session has at most one in-flight request; `send()`
  while in flight cancels the previous. Every request ends in exactly one of
  `finished` / `failed` / `cancelled`. Cancellation is not an error; no consumer
  string-matches a message to tell them apart.
- **Typed errors** — `ErrorInfo { category ∈ {Config, Auth, Network, Provider,
  Validation, Tool}, message, providerDetail }`. `ResponseRouter` categorizes wire
  errors (best-effort) at the boundary; `Session::failed` carries the typed value.
- **Tools** — `SessionManager` owns a `ToolContributorRegistry`; built-in ToolKit,
  the skill tool, and MCP client tools register once and are contributed to chat
  and quick-refactor session clients uniformly.
- **Threading** — the core runs on the GUI thread; concurrency is the Qt event
  loop plus async network I/O. Blocking work hides behind L3 ports.

---

## 8. Tests

`test/` (GTest + Qt::Test) covers the two engines most affected by the rework:

- `JsonPromptTemplateTest` — the `[body]` engine: jinja render + JSON splice,
  literal passthrough, empty-render key drop, nested literals, and load-time
  rejection of bodies that render invalid JSON.
- `ResponseRouterTest` — a fake `BaseClient` replays a recorded provider stream;
  asserts the assistant message is stamped with the request id, history is built
  correctly (thinking + text + tool use/result), the typed event stream is emitted,
  and wire errors are categorized.
- `BundledAgentsTest` — loads every bundled agent through the real loader (extends
  + partials resolved from the qrc) and renders each `[body]` against the synthetic
  validation context. This is the load-time validation guarantee run in CI: a broken
  bundled body, partial, or `extends` chain fails the test instead of surfacing as a
  silent runtime drop.

---

## 9. Remaining follow-ups (optional)

1. **Qt-Creator-free core build + CI** — `AgentFactory` / `ContextRenderer` still
   call `Core::ICore::userResourcePath`, so the core targets link `QtCreator::Core`.
   A `ResourcePaths` port + adapter would let the core build without Qt Creator and
   enable a CI job that fails on a layering-violating include. (The bundled-agent
   render check already runs in the QtC-linked test binary — see §8.)
2. **§9 target module layout** — the `core/ ide/ features/ hosts/` physical target
   split in `target-architecture.md` is not yet reflected in the directory layout.
```
