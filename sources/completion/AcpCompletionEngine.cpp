// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "completion/AcpCompletionEngine.hpp"

#include <chrono>

#include <LLMQore/AcpClient.hpp>
#include <LLMQore/CallbackPermissionProvider.hpp>

#include "acp/AgentSpawn.hpp"
#include "context/DocumentContextReader.hpp"
#include "logger/Logger.hpp"
#include "settings/SettingsConstants.hpp"

namespace QodeAssist {

namespace {

void upsertEnv(QList<Acp::AgentEnvVariable> &env, const QString &name, const QString &value)
{
    for (auto &variable : env) {
        if (variable.name == name) {
            variable.value = value;
            return;
        }
    }
    env.append({name, value});
}

LLMQore::Acp::RequestPermissionResult autoAllow(
    const QString &,
    const LLMQore::Acp::ToolCall &toolCall,
    const QList<LLMQore::Acp::PermissionOption> &options)
{
    QStringList optionDump;
    for (const auto &option : options)
        optionDump.append(QString("%1(%2)").arg(option.optionId, option.kind));

    const LLMQore::Acp::PermissionOption *chosen = nullptr;
    for (const auto &option : options) {
        if (option.kind == QLatin1String(LLMQore::Acp::PermissionOptionKind::AllowOnce)) {
            chosen = &option;
            break;
        }
        if (!chosen && option.kind == QLatin1String(LLMQore::Acp::PermissionOptionKind::AllowAlways))
            chosen = &option;
    }
    if (!chosen && !options.isEmpty())
        chosen = &options.first();

    LOG_MESSAGE(
        QString("ACP completion: permission requested for tool '%1' (kind=%2); options=[%3]; %4")
            .arg(
                toolCall.title,
                toolCall.kind,
                optionDump.join(QStringLiteral(", ")),
                chosen ? QString("auto-allowing '%1'").arg(chosen->optionId)
                       : QStringLiteral("cancelling (no option offered)")));

    return chosen ? LLMQore::Acp::RequestPermissionResult::selected(chosen->optionId)
                  : LLMQore::Acp::RequestPermissionResult::cancelled();
}

} // namespace

AcpCompletionEngine::AcpCompletionEngine(
    const Settings::CodeCompletionSettings &completeSettings,
    const Settings::GeneralSettings &generalSettings,
    AgentResolver agentResolver,
    Context::IDocumentReader &documentReader,
    Tools::ProposeCompletionTool *proposeTool,
    QObject *parent)
    : CompletionEngine(parent)
    , m_completeSettings(completeSettings)
    , m_generalSettings(generalSettings)
    , m_agentResolver(std::move(agentResolver))
    , m_documentReader(documentReader)
    , m_proposeTool(proposeTool)
    , m_clientFactory(&Acp::spawnAgent)
    , m_completionServer(new Mcp::CompletionMcpServer(this))
    , m_permissions(new LLMQore::Acp::CallbackPermissionProvider(&autoAllow, this))
{
    if (m_proposeTool) {
        connect(
            m_proposeTool,
            &Tools::ProposeCompletionTool::completionProposed,
            this,
            &AcpCompletionEngine::handleCompletionProposed);
    }
}

void AcpCompletionEngine::setClientFactory(ClientFactory factory)
{
    if (factory)
        m_clientFactory = std::move(factory);
}

bool AcpCompletionEngine::hasConfiguredAgent() const
{
    const QString agentId = m_completeSettings.completionAgentId();
    if (agentId.isEmpty() || !m_agentResolver)
        return false;
    return m_agentResolver(agentId).has_value();
}

void AcpCompletionEngine::request(quint64 requestId, const CompletionContext &context)
{
    if (m_active) {
        const auto stale = *m_active;
        m_active.reset();
        if (m_client && !m_sessionId.isEmpty())
            m_client->cancel(m_sessionId);
        emit completionFailed(stale.requestId, QStringLiteral("Superseded by a newer request"));
    }

    const QString agentId = m_completeSettings.completionAgentId();
    if (agentId.isEmpty()) {
        emit completionFailed(
            requestId, QStringLiteral("Agentic (ACP agent) completion has no agent selected"));
        return;
    }

    if (!m_agentResolver || !m_agentResolver(agentId).has_value()) {
        emit completionFailed(
            requestId, QString("ACP completion agent '%1' is not in the catalogue").arg(agentId));
        return;
    }

    auto documentInfo = m_documentReader.readDocument(context.filePath);
    if (!documentInfo.document) {
        emit completionFailed(
            requestId, QString("Document is not available: %1").arg(context.filePath));
        return;
    }

    Context::DocumentContextReader reader(
        documentInfo.document, documentInfo.mimeType, documentInfo.filePath);
    const auto ctx = reader.prepareContext(context.line, context.column, m_completeSettings);
    const QString codeContext = ctx.prefix.value_or("") + "<cursor>" + ctx.suffix.value_or("");

    m_active
        = ActiveRequest{requestId, documentInfo.filePath, context.line, context.column, codeContext};

    LOG_MESSAGE(
        QString("ACP completion: request %1 for %2 (line %3, col %4), agent '%5'")
            .arg(QString::number(requestId), documentInfo.filePath)
            .arg(context.line)
            .arg(context.column)
            .arg(agentId));

    if (m_boundAgentId != agentId) {
        if (!m_boundAgentId.isEmpty())
            LOG_MESSAGE(
                QString("ACP completion: selected agent changed %1 -> %2, releasing client")
                    .arg(m_boundAgentId, agentId));
        releaseClient();
    }
    m_boundAgentId = agentId;

    ensureClientAndPrompt();
}

void AcpCompletionEngine::ensureClientAndPrompt()
{
    if (m_client && m_sessionReady) {
        LOG_MESSAGE(
            QString("ACP completion: reusing live session %1, sending a new prompt")
                .arg(m_sessionId));
        sendPrompt();
        return;
    }

    if (m_client) {
        LOG_MESSAGE("ACP completion: client is still establishing its session, will prompt once ready");
        return;
    }

    const auto agent = m_agentResolver ? m_agentResolver(m_boundAgentId) : std::nullopt;
    if (!agent) {
        failActive(QString("ACP completion agent '%1' vanished from the catalogue").arg(m_boundAgentId));
        return;
    }

    Acp::AgentDefinition launchAgent = *agent;
    const QString model = m_generalSettings.ccModel();
    if (!model.isEmpty()) {
        upsertEnv(launchAgent.distribution.env, QStringLiteral("ANTHROPIC_MODEL"), model);
        LOG_MESSAGE(
            QString("ACP completion: injecting ANTHROPIC_MODEL=%1 into agent env").arg(model));
    }

    const QString cwd = Acp::agentWorkingDirectory();
    LOG_MESSAGE(
        QString("ACP completion: spawning agent '%1' (%2) in %3")
            .arg(launchAgent.id, launchAgent.name, cwd));
    const Acp::AgentProcess process = m_clientFactory(launchAgent, cwd, this);
    m_client = process.client;
    if (!m_client) {
        failActive(QStringLiteral("This agent has no launchable distribution"));
        return;
    }

    m_client->setPermissionProvider(m_permissions);

    const int generation = ++m_clientGeneration;
    connect(m_client, &LLMQore::Acp::AcpClient::errorOccurred, this, [this, generation](const QString &error) {
        if (generation != m_clientGeneration)
            return;
        LOG_MESSAGE(QString("ACP completion: agent error: %1").arg(error));
        failActive(error);
        releaseClient();
    });
    connect(m_client, &LLMQore::Acp::AcpClient::disconnected, this, [this, generation]() {
        if (generation != m_clientGeneration)
            return;
        LOG_MESSAGE("ACP completion: agent disconnected");
        m_sessionReady = false;
        m_sessionId.clear();
    });
    connect(m_client, &LLMQore::Acp::AcpClient::agentStderr, this, [this, generation](const QString &line) {
        if (generation != m_clientGeneration)
            return;
        LOG_MESSAGE(QString("ACP completion [agent stderr]: %1").arg(line));
    });
    connect(
        m_client,
        &LLMQore::Acp::AcpClient::toolCallStarted,
        this,
        [this, generation](const QString &, const LLMQore::Acp::ToolCall &call) {
            if (generation != m_clientGeneration)
                return;
            LOG_MESSAGE(
                QString("ACP completion: agent tool call started: title='%1' kind=%2 status=%3")
                    .arg(call.title, call.kind, call.status));
        });
    connect(
        m_client,
        &LLMQore::Acp::AcpClient::toolCallUpdated,
        this,
        [this, generation](const QString &, const LLMQore::Acp::ToolCall &call) {
            if (generation != m_clientGeneration)
                return;
            LOG_MESSAGE(
                QString("ACP completion: agent tool call updated: title='%1' kind=%2 status=%3")
                    .arg(call.title, call.kind, call.status));
        });

    LOG_MESSAGE("ACP completion: connecting and initializing the agent");
    m_client->connectAndInitialize(std::chrono::seconds(30))
        .then(this, [this, generation, cwd](const LLMQore::Acp::InitializeResult &info) {
            if (generation != m_clientGeneration || !m_client)
                return;
            const bool http = info.agentCapabilities.mcpCapabilities.http;
            LOG_MESSAGE(
                QString("ACP completion: agent initialized (protocol %1, MCP http=%2 sse=%3)")
                    .arg(QString::number(info.protocolVersion),
                         http ? QStringLiteral("true") : QStringLiteral("false"),
                         info.agentCapabilities.mcpCapabilities.sse ? QStringLiteral("true")
                                                                    : QStringLiteral("false")));
            if (!http) {
                LOG_MESSAGE(
                    "ACP completion: WARNING — this agent does not advertise HTTP MCP support, so "
                    "it will likely ignore the QodeAssist MCP server and never call "
                    "propose_completion. Use an agent that accepts HTTP MCP servers.");
            }

            const QString mcpUrl = m_completionServer->start(m_proposeTool);
            if (mcpUrl.isEmpty()) {
                failActive(QStringLiteral(
                    "The QodeAssist completion MCP server could not start; ACP completion needs it"));
                releaseClient();
                return;
            }
            LLMQore::Acp::NewSessionParams params;
            params.cwd = cwd;
            params.mcpServers
                = {LLMQore::Acp::McpServer::http(Mcp::CompletionMcpServer::serverName(), mcpUrl)};
            LOG_MESSAGE(
                QString("ACP completion: creating session, offering MCP server '%1' at %2")
                    .arg(Mcp::CompletionMcpServer::serverName(), mcpUrl));
            m_client->newSession(params, std::chrono::seconds(30))
                .then(this, [this, generation](const LLMQore::Acp::NewSessionResult &result) {
                    if (generation != m_clientGeneration || !m_client)
                        return;
                    m_sessionId = result.sessionId;
                    m_sessionReady = true;
                    LOG_MESSAGE(QString("ACP completion: session %1 established").arg(m_sessionId));
                    sendPrompt();
                })
                .onFailed(this, [this, generation](const std::exception &e) {
                    if (generation != m_clientGeneration)
                        return;
                    LOG_MESSAGE(QString("ACP completion: session/new failed: %1")
                                    .arg(QString::fromUtf8(e.what())));
                    failActive(QString::fromUtf8(e.what()));
                    releaseClient();
                });
        })
        .onFailed(this, [this, generation](const std::exception &e) {
            if (generation != m_clientGeneration)
                return;
            LOG_MESSAGE(QString("ACP completion: initialize failed: %1")
                            .arg(QString::fromUtf8(e.what())));
            failActive(QString::fromUtf8(e.what()));
            releaseClient();
        });
}

void AcpCompletionEngine::sendPrompt()
{
    if (!m_active || !m_client || m_sessionId.isEmpty())
        return;

    const int generation = m_clientGeneration;
    const quint64 activeId = m_active->requestId;
    const QString text = promptText();
    LOG_MESSAGE(
        QString("ACP completion: prompting session %1 for request %2")
            .arg(m_sessionId, QString::number(activeId)));
    LOG_MESSAGE(QString("ACP completion: prompt text:\n%1").arg(text));
    m_client->prompt(m_sessionId, {LLMQore::Acp::ContentBlock::makeText(text)})
        .then(this, [this, generation, activeId](const LLMQore::Acp::PromptResult &result) {
            if (generation != m_clientGeneration || !m_active || m_active->requestId != activeId)
                return;
            LOG_MESSAGE(
                QString("ACP completion: prompt for request %1 finished with stopReason '%2' but "
                        "no proposal arrived")
                    .arg(QString::number(activeId), result.stopReason));
            m_active.reset();
            emit completionFailed(
                activeId,
                QStringLiteral("The agent finished without proposing a completion"));
        })
        .onFailed(this, [this, generation, activeId](const std::exception &e) {
            if (generation != m_clientGeneration || !m_active || m_active->requestId != activeId)
                return;
            m_active.reset();
            LOG_MESSAGE(QString("ACP completion failed: %1").arg(QString::fromUtf8(e.what())));
            emit completionFailed(activeId, QString::fromUtf8(e.what()));
        });
}

QString AcpCompletionEngine::promptText() const
{
    if (!m_active)
        return {};
    return QString(
               "Complete the code at the <cursor> marker in the snippet below. Be fast: do NOT "
               "use any other tools, do NOT read files, run commands, or search — work only "
               "from this snippet.\n\n"
               "<code_context>\n%1\n</code_context>\n\n"
               "First work out the exact code that should replace <cursor> (just the "
               "continuation — no explanations, no code fences, and do not repeat the code "
               "already before or after the cursor). Then make ONE call to propose_completion "
               "with that code as the `text` argument. The `text` must be the real completion "
               "and must never be empty. file=%2, line=%3, column=%4.")
        .arg(m_active->codeContext)
        .arg(m_active->filePath)
        .arg(m_active->line)
        .arg(m_active->column);
}

void AcpCompletionEngine::cancel(quint64 requestId)
{
    if (!m_active || m_active->requestId != requestId)
        return;
    m_active.reset();
    if (m_client && !m_sessionId.isEmpty())
        m_client->cancel(m_sessionId);
}

void AcpCompletionEngine::handleCompletionProposed(
    const QString &filePath, int line, int column, const QString &text)
{
    Q_UNUSED(line)
    Q_UNUSED(column)

    LOG_MESSAGE(
        QString("ACP completion: propose_completion called for '%1' (%2 chars)")
            .arg(filePath, QString::number(text.size())));

    if (!m_active) {
        LOG_MESSAGE("ACP completion: ignoring proposal — no completion request is active");
        return;
    }

    if (m_active->filePath != filePath) {
        LOG_MESSAGE(
            QString("ACP completion: proposal path '%1' differs from the requested '%2'; "
                    "accepting anyway (single active request)")
                .arg(filePath, m_active->filePath));
    }

    const quint64 externalId = m_active->requestId;
    m_active.reset();
    LOG_MESSAGE(
        QString("ACP completion: rendering proposal for request %1").arg(QString::number(externalId)));
    emit completionReady(externalId, text);
}

void AcpCompletionEngine::failActive(const QString &error)
{
    if (!m_active)
        return;
    const auto stale = *m_active;
    m_active.reset();
    LOG_MESSAGE(QString("ACP completion failed: %1").arg(error));
    emit completionFailed(stale.requestId, error);
}

void AcpCompletionEngine::releaseClient()
{
    ++m_clientGeneration;
    m_sessionReady = false;
    m_sessionId.clear();
    if (m_client) {
        m_client->shutdown();
        m_client->deleteLater();
        m_client = nullptr;
    }
    if (m_completionServer)
        m_completionServer->stop();
}

} // namespace QodeAssist
