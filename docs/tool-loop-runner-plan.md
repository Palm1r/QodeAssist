# ToolLoopRunner — implementation plan

Status: plan for "variant C" (2026-06-13). Supersedes step 5 of
`context-architecture.md` §6.

Context that shapes this plan:
- The tool loop STAYS in LLMQore — the library remains a complete standalone
  agentic client. Variant C changes its *shape*, not its home: the loop
  becomes a named class, `BaseClient` slims toward transport.
- 2026-06-12 the variant-A hook (`setContinuationPayloadBuilder` + Session
  feeding assembler-built continuation bodies) was implemented and then
  REVERTED by the project owner: the frozen-replay problem was judged
  contrived (replay carries the full filtered history of its base payload;
  mid-loop file changes reach the model via tool results; growth is bounded
  by `maxToolContinuations`). The reverted llmqore diff is saved at
  `/tmp/llmqore-continuation-builder.patch`.
- Therefore this plan has two tracks. **Track 1 (the actual ask): the
  structural refactoring.** Track 2 (host payload source) is OPTIONAL,
  parked, and only happens if the 2026-06-12 verdict is explicitly reversed.
- The context-architecture steps 1–4 implementation (ContextAssembler,
  content cache, pinned providers, EnvBlockFormatter, ~1200 lines incl.
  tests) is parked in `stash@{0}` ("new context refactor") on
  `dev-release-1-0`. It is NOT required for track 1.

---

## 1. Current anatomy (llmqore @ 0348ac8)

- `BaseClient` mixes two responsibilities:
  - **transport** — HTTP/SSE per request, `ActiveRequest { stream, buffers,
    url, mode, usage, … }`, accumulation in protocol subclasses;
  - **loop policy** — `ActiveRequest.originalPayload`,
    `ActiveRequest.continuationCount`, `m_maxToolContinuations`,
    `checkContinuationLimit`, `handleToolContinuation`.
- Loop entry: protocol clients call `executeToolsFromMessage(id)` at their
  message-end detection points (11 call sites across 7 clients); it forwards
  `tool_use` blocks to `ToolsManager::executeToolCall`.
- `BaseClient::tools()` wires `ToolsManager::toolExecutionComplete(id,
  results)` → `handleToolContinuation`: round-limit check → continuation
  body via the protocol-virtual `buildContinuationPayload(originalPayload,
  message, toolResults)` → `finalizeTurn` → `sendRequest(id, storedUrl,
  payload, storedMode)`.

## 2. Target design

### 2.1 ToolLoopRunner (new, llmqore)

```cpp
class LLMQORE_EXPORT ToolLoopRunner : public QObject
{
    Q_OBJECT
public:
    explicit ToolLoopRunner(BaseClient *client);

    int maxRounds() const noexcept;
    void setMaxRounds(int limit) noexcept;

private:
    void onToolsCompleted(const RequestID &id,
                          const QHash<QString, ToolResult> &results);
    void onRequestClosed(const RequestID &id);

    struct LoopState
    {
        int rounds = 0;
    };

    BaseClient *m_client = nullptr;
    QHash<RequestID, LoopState> m_loops;
    int m_maxRounds = 10;
};
```

The whole loop policy on one screen:

```cpp
void ToolLoopRunner::onToolsCompleted(const RequestID &id,
                                      const QHash<QString, ToolResult> &results)
{
    auto &loop = m_loops[id];
    if (++loop.rounds > m_maxRounds) {
        m_client->abortRequest(id, "Tool continuation limit reached");
        m_loops.remove(id);
        return;
    }

    const QJsonObject payload = m_client->buildReplayContinuation(id, results);
    if (payload.isEmpty()) {
        m_client->abortRequest(id, "Failed to build continuation payload");
        m_loops.remove(id);
        return;
    }

    m_client->continueRequest(id, payload);
}
```

- `LoopState` is keyed by request id — several concurrent requests on one
  client (two chat panels on one provider) never collide.
- Cleanup: `onRequestClosed` (connected to `requestFailed` +
  `requestFinalized`) drops the state.

### 2.2 BaseClient becomes transport + tool dispatch

Gains (transport primitives; `continueRequest` public — it is also the seam
any future host-driven mode would use; failure path via runner friendship):

```cpp
ToolLoopRunner *toolLoop();                       // owned, created with tools()
void continueRequest(const RequestID &id, const QJsonObject &payload);
                                                  // finalizeTurn + resend stored url/mode
QJsonObject buildReplayContinuation(const RequestID &id,
                                    const QHash<QString, ToolResult> &results);
                                                  // originalPayload + protocol virtual
```

Loses (moves to the runner): `handleToolContinuation`,
`checkContinuationLimit`, `m_maxToolContinuations`,
`ActiveRequest::continuationCount`. The `toolExecutionComplete` connection
in `tools()` retargets to the runner.

Keeps: `executeToolsFromMessage` (the 11 protocol call sites stay
untouched), the protocol-virtual `buildContinuationPayload` (it IS the
replay serialization), `originalPayload` storage,
`setMaxToolContinuations`/`maxToolContinuations` as thin forwarders to
`toolLoop()` — existing consumers (QodeAssist `ClientInterface`,
`QuickRefactorHandler`, third parties) compile unchanged.

## 3. Track 1 — structural refactoring (the plan)

Bit-identical behavior throughout; QodeAssist only needs a submodule bump.

**Phase 1 — transport primitives.** Add `continueRequest` +
`buildReplayContinuation` + public `abortRequest` (now also the body of
`cancelRequest`). — DONE 2026-06-13.

**Phase 2 — extract the runner.** New `ToolLoopRunner` class; move round
state + limit; retarget the `toolExecutionComplete` connection; delete
`handleToolContinuation` / `checkContinuationLimit` /
`ActiveRequest::continuationCount`; forwarders for
`setMaxToolContinuations`. — DONE 2026-06-13
(`include/LLMQore/ToolLoopRunner.hpp`, `source/core/ToolLoopRunner.cpp`,
`tests/tst_ToolLoopRunner.cpp` — 7 cases: replay flow, round limit, missing
replay data, two interleaved ids, cleanup on finalize/cancel, forwarders;
`continueRequest` is virtual as the test seam; llmqore architecture docs
updated: overview, request-lifecycle diagram, tools).

Deliberate behavior delta (an improvement, worth knowing while testing): an
empty payload from the protocol's `buildContinuationPayload` now aborts the
request with "Missing data for tool continuation" instead of silently
sending an empty body.

**Phase 3 — submodule bump (after the user runs llmqore tests).**
QodeAssist: bump the submodule pointer, verify live in the plugin (Ollama +
tools, Claude + tools); update `context-architecture.md`
§4.3/§6.5 to point here; update project memory.

## 4. Track 2 — host payload source (PARKED)

Only if the 2026-06-12 "проблема надумана" verdict is explicitly reversed.
Variant C makes it a ~40-line addition, so nothing is lost by parking:

- `ToolLoopRunner::setPayloadSource(id, std::function<QJsonObject(const
  RequestID &)>)`; registered source is authoritative for its id (empty
  result → abort, never silent fallback to replay).
- Host prerequisite: restore the context work from `stash@{0}`
  (ContextAssembler + `Session::makePayload`); expect conflicts in
  `Session.cpp` with the newer `dev-release-1-0` refactor commits
  ("Remove override tools in Session send" etc.).
- Session registers the source after `provider->sendRequest` (same-thread,
  race-free; `QPointer` guard).
- Assembler continuation rules: pinned blocks anchor to the turn's TYPED
  user message (recorded 2026-06-12), manifest per round.

## 5. Risks

| Risk | Mitigation |
|---|---|
| Behavior drift while moving the loop | phases are mechanical; same `buildContinuationPayload` virtuals; llmqore tests + plugin smoke before/after |
| Two sessions, one client | `LoopState` keyed by request id |
| Qt 5 compatibility (0348ac8) | runner uses only signals/`QHash`/`std::function` — no Qt 6-only API |
| Cancel mid-tool-execution | unchanged: `cancelRequest` → `failRequest` → `onRequestClosed` clears state; `ToolsManager::cleanupRequest` handles in-flight tools |
| Google (model in URL) | `continueRequest` reuses stored per-request url/mode — same as today |

## 6. Deliberately not doing

- Not moving the loop or tool execution out of llmqore
  (`feedback_llmqore_boundary`).
- Not touching the 11 `executeToolsFromMessage` call sites or the protocol
  `buildContinuationPayload` implementations.
- No Auto/Manual mode flags.
- Track 2 is not started without an explicit decision.
