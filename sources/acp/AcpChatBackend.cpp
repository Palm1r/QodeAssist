// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AcpChatBackend.hpp"

#include <chrono>

#include <QJsonArray>
#include <QMimeDatabase>
#include <QSet>
#include <QUrl>

#include <LLMQore/AcpClient.hpp>
#include <LLMQore/RpcExceptions.hpp>

#include <logger/Logger.hpp>
#include <session/FencedText.hpp>

namespace QodeAssist::Acp {

namespace {

constexpr int authRequiredErrorCode = -32000;
constexpr int maxStderrLines = 20;
constexpr qsizetype maxInlineAttachmentBytes = 512 * 1024;
constexpr qsizetype maxInlineImageBytes = 8 * 1024 * 1024;
constexpr qsizetype maxPermissionOptions = 16;
constexpr qsizetype maxPermissionTitleLength = 300;
constexpr qsizetype maxPermissionKindLength = 40;
constexpr qsizetype maxPermissionOptionNameLength = 120;
constexpr qsizetype maxAgentCommands = 128;
constexpr qsizetype maxAgentCommandNameLength = 64;
constexpr qsizetype maxToolResultLength = 64 * 1024;
constexpr qsizetype maxToolDiffLength = 64 * 1024;
constexpr qsizetype maxToolLocations = 64;
constexpr qsizetype maxToolDiffs = 16;
constexpr qsizetype maxAgentTitleLength = 60;

QString clampAgentText(const QString &text, qsizetype limit)
{
    if (text.size() <= limit)
        return text;
    return text.first(limit - 1) + QChar(0x2026);
}

QString truncationNotice(qsizetype shown, qsizetype total)
{
    return QLatin1Char('\n')
           + QObject::tr("[truncated by QodeAssist: %1 of %2 bytes shown]").arg(shown).arg(total);
}

QString clampAgentBody(const QString &text, qsizetype limit)
{
    if (text.size() <= limit)
        return text;
    return text.first(limit) + truncationNotice(limit, text.size());
}

QString singleLineAgentText(const QString &text, qsizetype limit)
{
    QString line;
    line.reserve(qMin(text.size(), limit));

    for (const QChar c : text) {
        if (c.isSpace()) {
            if (!line.isEmpty() && !line.endsWith(QLatin1Char(' ')))
                line.append(QLatin1Char(' '));
            continue;
        }
        if (c.category() == QChar::Other_Control || c.category() == QChar::Other_Format)
            continue;
        line.append(c);
    }

    return clampAgentText(line.trimmed(), limit);
}

QString validatePermissionOptions(const QList<LLMQore::Acp::PermissionOption> &options)
{
    if (options.isEmpty())
        return QStringLiteral("the agent offered no options to choose from");

    if (options.size() > maxPermissionOptions) {
        return QStringLiteral("the agent offered %1 options, more than the %2 allowed")
            .arg(options.size())
            .arg(maxPermissionOptions);
    }

    QSet<QString> seen;
    for (const LLMQore::Acp::PermissionOption &option : options) {
        if (option.optionId.isEmpty())
            return QStringLiteral("the agent offered an option with no id");
        if (seen.contains(option.optionId))
            return QStringLiteral("the agent offered option id %1 twice").arg(option.optionId);
        seen.insert(option.optionId);
    }

    return {};
}

QString attachmentUri(const QString &fileName)
{
    return QStringLiteral("qodeassist://attachment/")
           + QString::fromUtf8(QUrl::toPercentEncoding(fileName));
}

QMimeType mimeTypeForFileName(const QString &fileName)
{
    return QMimeDatabase().mimeTypeForFile(fileName, QMimeDatabase::MatchExtension);
}

QString textMimeTypeForFileName(const QString &fileName)
{
    const QMimeType type = mimeTypeForFileName(fileName);
    return type.inherits(QStringLiteral("text/plain")) ? type.name() : QStringLiteral("text/plain");
}

LLMQore::Acp::ContentBlock makeResource(
    const QString &uri, const QString &mimeType, const QString &text)
{
    LLMQore::Acp::EmbeddedResource resource;
    resource.uri = uri;
    resource.mimeType = mimeType;
    resource.text = text;

    LLMQore::Acp::ContentBlock block;
    block.type = QStringLiteral("resource");
    block.resource = resource;
    return block;
}

LLMQore::Acp::ContentBlock makeImage(const QString &mediaType, const QString &base64Data)
{
    LLMQore::Acp::ContentBlock block;
    block.type = QStringLiteral("image");
    block.mimeType = mediaType;
    block.data = base64Data;
    return block;
}

bool hasSendableContent(const QList<Session::ContentBlock> &userBlocks)
{
    for (const Session::ContentBlock &block : userBlocks) {
        if (const auto *text = std::get_if<Session::TextBlock>(&block)) {
            if (!text->text.isEmpty())
                return true;
            continue;
        }
        if (std::get_if<Session::AttachmentBlock>(&block)
            || std::get_if<Session::ImageBlock>(&block)) {
            return true;
        }
    }
    return false;
}

QString textOf(const LLMQore::Acp::ContentBlock &block)
{
    return block.type == QLatin1String("text") ? block.text : QString();
}

int tokenCount(const QJsonValue &value)
{
    qint64 count = -1;

    if (value.isDouble()) {
        const double raw = value.toDouble(-1.0);
        if (raw >= 0.0 && raw == std::floor(raw))
            count = static_cast<qint64>(raw);
    } else if (value.isString()) {
        bool parsed = false;
        const qint64 raw = value.toString().toLongLong(&parsed);
        if (parsed && raw >= 0)
            count = raw;
    }

    if (count < 0)
        return 0;

    return static_cast<int>(qMin<qint64>(count, std::numeric_limits<int>::max()));
}

int usageValue(const QJsonObject &usage, QLatin1StringView camelCase, QLatin1StringView snakeCase)
{
    const int fromCamelCase = tokenCount(usage.value(camelCase));
    return fromCamelCase > 0 ? fromCamelCase : tokenCount(usage.value(snakeCase));
}

Session::Usage usageFromPromptResult(const QJsonObject &usage)
{
    return Session::Usage{
        .promptTokens = usageValue(usage, QLatin1StringView("inputTokens"),
                                   QLatin1StringView("input_tokens")),
        .completionTokens = usageValue(usage, QLatin1StringView("outputTokens"),
                                       QLatin1StringView("output_tokens")),
        .cachedPromptTokens = usageValue(usage, QLatin1StringView("cachedReadTokens"),
                                         QLatin1StringView("cached_read_tokens")),
        .reasoningTokens = 0};
}

QString describeContentBlock(const LLMQore::Acp::ContentBlock &block)
{
    if (block.type == QLatin1String("text"))
        return block.text;

    if (block.type == QLatin1String("resource") && block.resource) {
        return block.resource->text.isEmpty()
                   ? QObject::tr("[resource %1]").arg(block.resource->uri)
                   : block.resource->text;
    }

    if (block.type == QLatin1String("resource_link"))
        return QObject::tr("[link %1]").arg(block.uri);

    if (block.type == QLatin1String("image") || block.type == QLatin1String("audio"))
        return QObject::tr("[%1 result, not shown]").arg(block.type);

    LOG_MESSAGE(QString("Unhandled ACP tool result content type: %1").arg(block.type));
    return QObject::tr("[%1 result, not shown]").arg(block.type);
}

QString toolResultText(const LLMQore::Acp::ToolCall &toolCall)
{
    QStringList parts;

    for (const LLMQore::Acp::ToolCallContent &content : toolCall.content) {
        if (content.type == QLatin1String("content")) {
            if (content.content)
                parts.append(describeContentBlock(*content.content));
        } else if (content.type == QLatin1String("terminal")) {
            parts.append(QObject::tr("Terminal %1").arg(content.terminalId));
        } else if (content.type != QLatin1String("diff")) {
            LOG_MESSAGE(QString("Unhandled ACP tool call content type: %1").arg(content.type));
        }
    }

    parts.removeAll(QString());
    return clampAgentBody(parts.join(QLatin1Char('\n')), maxToolResultLength);
}

QJsonObject toolDetailsOf(const LLMQore::Acp::ToolCall &toolCall)
{
    QJsonArray locations;
    for (const LLMQore::Acp::ToolCallLocation &location : toolCall.locations) {
        if (locations.size() >= maxToolLocations) {
            LOG_MESSAGE(QString("Dropping %1 extra locations reported by tool call %2")
                            .arg(toolCall.locations.size() - maxToolLocations)
                            .arg(toolCall.toolCallId));
            break;
        }
        QJsonObject entry{{"path", singleLineAgentText(location.path, maxPermissionTitleLength)}};
        if (location.line)
            entry["line"] = *location.line;
        locations.append(entry);
    }

    QJsonArray diffs;
    for (const LLMQore::Acp::ToolCallContent &content : toolCall.content) {
        if (content.type != QLatin1String("diff"))
            continue;
        if (diffs.size() >= maxToolDiffs) {
            LOG_MESSAGE(
                QString("Dropping extra diffs reported by tool call %1").arg(toolCall.toolCallId));
            break;
        }
        QJsonObject diff{
            {"path", singleLineAgentText(content.path, maxPermissionTitleLength)},
            {"oldText", clampAgentBody(content.oldText, maxToolDiffLength)},
            {"newText", clampAgentBody(content.newText, maxToolDiffLength)}};
        if (content.oldText.size() > maxToolDiffLength
            || content.newText.size() > maxToolDiffLength) {
            diff["truncated"] = true;
        }
        diffs.append(diff);
    }

    QJsonObject details;
    if (!locations.isEmpty())
        details["locations"] = locations;
    if (!diffs.isEmpty())
        details["diffs"] = diffs;
    return details;
}

} // namespace

AcpChatBackend::AcpChatBackend(QObject *parent)
    : Session::ChatBackend(parent)
    , m_clientFactory(&spawnAgent)
    , m_permissions(new ChatPermissionProvider(&m_ledger, this))
{
    m_permissions->setRequestHandler(
        [this](
            const QString &requestId,
            const LLMQore::Acp::ToolCall &toolCall,
            const QList<LLMQore::Acp::PermissionOption> &options) {
            requestPermission(requestId, toolCall, options);
        });
}

void AcpChatBackend::setClientFactory(ClientFactory factory)
{
    m_clientFactory = std::move(factory);
}

void AcpChatBackend::setStoredContentLoader(StoredContentLoader loader)
{
    m_storedContentLoader = std::move(loader);
}

void AcpChatBackend::setKnowledgeService(AgentKnowledgeService *service)
{
    m_knowledgeService = service;
}

QList<LLMQore::Acp::McpServer> AcpChatBackend::knowledgeServers()
{
    if (!m_knowledgeService)
        return {};

    if (!m_agentInfo.agentCapabilities.mcpCapabilities.http) {
        LOG_MESSAGE(
            QString("Agent %1 does not accept HTTP MCP servers, so QodeAssist knowledge is not "
                    "offered")
                .arg(m_agent ? m_agent->id : QString()));
        return {};
    }

    const QString url = m_knowledgeService->start();
    if (url.isEmpty()) {
        LOG_MESSAGE("The QodeAssist knowledge server did not start, continuing without it");
        return {};
    }

    m_knowledgeServerRunning = true;
    LOG_MESSAGE(QString("Offering the agent the QodeAssist knowledge server at %1").arg(url));

    return {LLMQore::Acp::McpServer::http(m_knowledgeService->serverName(), url)};
}

void AcpChatBackend::stopKnowledgeServer()
{
    if (!m_knowledgeServerRunning)
        return;

    m_knowledgeServerRunning = false;
    if (m_knowledgeService)
        m_knowledgeService->stop();
}

void AcpChatBackend::setChatFilePath(const QString &filePath)
{
    m_chatFilePath = filePath;
}

void AcpChatBackend::bindAgent(const AgentDefinition &agent)
{
    if (m_agent && m_agent->id == agent.id)
        return;

    cancel();
    releaseClient();

    m_agent = agent;
    m_sessionId.clear();
    m_resumeSessionId.clear();
    m_handoverSummary.clear();
    m_authenticated = false;
}

QString AcpChatBackend::boundAgentId() const
{
    return m_agent ? m_agent->id : QString();
}

void AcpChatBackend::adoptEarlyCommands()
{
    if (!m_earlyCommandsSessionId.isEmpty() && m_earlyCommandsSessionId == m_sessionId) {
        m_availableCommands = m_earlyCommands;
        emit availableCommandsChanged();
    }
    m_earlyCommands.clear();
    m_earlyCommandsSessionId.clear();

    if (!m_earlyTitleSessionId.isEmpty() && m_earlyTitleSessionId == m_sessionId
        && !m_earlyTitle.isEmpty()) {
        emit sessionEvent(Session::SessionInfo{.title = m_earlyTitle});
    }
    m_earlyTitle.clear();
    m_earlyTitleSessionId.clear();
}

QString AcpChatBackend::boundAgentName() const
{
    return m_agent ? singleLineAgentText(m_agent->name, maxPermissionKindLength) : QString();
}

void AcpChatBackend::sendTurn(const Session::TurnRequest &request)
{
    if (!m_agent) {
        emit sessionEvent(
            Session::TurnFailed{.turnId = {}, .error = tr("No agent is bound to this chat.")});
        return;
    }

    cancel();

    if (!hasSendableContent(request.userBlocks)) {
        emit sessionEvent(
            Session::TurnFailed{.turnId = {}, .error = tr("There is nothing to send to the agent.")});
        return;
    }

    m_pendingTurn = PendingTurn{request.userBlocks};

    const QString turnId = m_ledger.beginTurn(QStringLiteral("acp-%1").arg(++m_turnCounter));
    m_stderr.clear();
    emit sessionEvent(Session::TurnStarted{.turnId = turnId});

    if (!m_client) {
        startClient();
        return;
    }

    if (m_sessionId.isEmpty()) {
        if (!m_establishingSession)
            resumeOrStartSession();
        return;
    }

    sendPrompt();
}

void AcpChatBackend::startClient()
{
    m_workingDirectory = agentWorkingDirectory();

    const AgentProcess process = m_clientFactory(*m_agent, m_workingDirectory, this);
    m_client = process.client;
    m_runner = process.command;

    if (!m_client) {
        failTurn(tr("This agent has no launchable distribution."), true);
        return;
    }

    m_client->setPermissionProvider(m_permissions);
    connectClient();

    const int generation = m_clientGeneration;

    m_client->connectAndInitialize(std::chrono::seconds(60))
        .then(
            this,
            [this, generation](const LLMQore::Acp::InitializeResult &result) {
                if (generation != m_clientGeneration)
                    return;
                m_agentInfo = result;
                if (m_ledger.hasActiveTurn())
                    resumeOrStartSession();
            })
        .onFailed(this, [this, generation](const std::exception &e) {
            if (generation != m_clientGeneration)
                return;
            failTurn(QString::fromUtf8(e.what()), true);
        });
}

void AcpChatBackend::resumeSession(const QString &sessionId)
{
    m_resumeSessionId = sessionId;
}

void AcpChatBackend::startFreshSession()
{
    m_resumeSessionId.clear();
}

void AcpChatBackend::setHandoverSummary(const QString &summary)
{
    m_handoverSummary = summary;
}

void AcpChatBackend::resumeOrStartSession()
{
    if (m_resumeSessionId.isEmpty()) {
        startSession();
        return;
    }

    const QString resumed = m_resumeSessionId;

    if (!m_agentInfo.agentCapabilities.loadSession) {
        m_resumeSessionId.clear();
        failTurn(
            tr("This agent cannot reopen a previous conversation, so the transcript below is "
               "read-only. Start a new session to continue with this agent."),
            false);
        emit agentSessionUnavailable(
            tr("%1 does not support reopening a session.")
                .arg(m_agent ? m_agent->name : tr("This agent")));
        return;
    }

    LLMQore::Acp::LoadSessionParams params;
    params.sessionId = resumed;
    params.cwd = m_workingDirectory;
    params.mcpServers = knowledgeServers();

    const int generation = m_clientGeneration;
    m_establishingSession = true;

    m_client->loadSession(params, std::chrono::seconds(60))
        .then(
            this,
            [this, resumed, generation](const LLMQore::Acp::NewSessionResult &result) {
                if (generation != m_clientGeneration)
                    return;
                m_establishingSession = false;
                m_resumeSessionId.clear();
                m_sessionId = result.sessionId.isEmpty() ? resumed : result.sessionId;
                adoptEarlyCommands();
                if (m_ledger.hasActiveTurn())
                    sendPrompt();
            })
        .onFailed(this, [this, generation](const std::exception &e) {
            if (generation != m_clientGeneration)
                return;
            m_establishingSession = false;
            m_resumeSessionId.clear();
            stopKnowledgeServer();
            const QString reason = QString::fromUtf8(e.what());
            LOG_MESSAGE(QString("Could not reopen the agent session: %1").arg(reason));
            failTurn(
                tr("The previous agent session could not be reopened, so the transcript below is "
                   "read-only. Start a new session to continue with this agent."),
                false);
            emit agentSessionUnavailable(reason);
        });
}

void AcpChatBackend::startSession()
{
    LLMQore::Acp::NewSessionParams params;
    params.cwd = m_workingDirectory;
    params.mcpServers = knowledgeServers();

    const int generation = m_clientGeneration;
    m_establishingSession = true;

    m_client->newSession(params, std::chrono::seconds(60))
        .then(
            this,
            [this, generation](const LLMQore::Acp::NewSessionResult &result) {
                if (generation != m_clientGeneration)
                    return;
                m_establishingSession = false;
                m_sessionId = result.sessionId;
                adoptEarlyCommands();
                if (m_ledger.hasActiveTurn())
                    sendPrompt();
            })
        .onFailed(
            this,
            [this, generation](const LLMQore::Rpc::RemoteError &error) {
                if (generation != m_clientGeneration)
                    return;
                m_establishingSession = false;
                const bool needsAuthentication = error.code() == authRequiredErrorCode
                                                 && !m_authenticated
                                                 && !m_agentInfo.authMethods.isEmpty();
                if (needsAuthentication)
                    authenticateAndRetry();
                else
                    failTurn(error.remoteMessage(), true);
            })
        .onFailed(this, [this, generation](const std::exception &e) {
            if (generation != m_clientGeneration)
                return;
            m_establishingSession = false;
            failTurn(QString::fromUtf8(e.what()), true);
        });
}

void AcpChatBackend::authenticateAndRetry()
{
    const LLMQore::Acp::AuthMethod method = m_agentInfo.authMethods.first();
    LOG_MESSAGE(QString("ACP agent requires authentication, using method: %1").arg(method.id));

    m_authenticated = true;

    const int generation = m_clientGeneration;
    const QString turnId = m_ledger.activeTurnId();

    m_client->authenticate(method.id, std::chrono::minutes(5))
        .then(
            this,
            [this, generation, turnId]() {
                if (generation != m_clientGeneration || !m_ledger.isActiveTurn(turnId))
                    return;
                resumeOrStartSession();
            })
        .onFailed(this, [this, generation, turnId, method](const std::exception &e) {
            if (generation != m_clientGeneration || !m_ledger.isActiveTurn(turnId))
                return;
            failTurn(
                tr("Authentication (%1) failed: %2")
                    .arg(method.name.isEmpty() ? method.id : method.name)
                    .arg(QString::fromUtf8(e.what())),
                true);
        });
}

QList<LLMQore::Acp::ContentBlock> AcpChatBackend::buildPrompt() const
{
    QList<LLMQore::Acp::ContentBlock> blocks;

    if (!m_handoverSummary.isEmpty()) {
        blocks.append(
            LLMQore::Acp::ContentBlock::makeText(
                tr("This conversation continues an earlier session you do not have. Here is a "
                   "summary of what happened before, prepared by QodeAssist:\n\n%1")
                    .arg(m_handoverSummary)));
    }

    for (const Session::ContentBlock &block : m_pendingTurn.userBlocks) {
        if (const auto *text = std::get_if<Session::TextBlock>(&block)) {
            if (!text->text.isEmpty())
                blocks.append(LLMQore::Acp::ContentBlock::makeText(text->text));
        } else if (const auto *attachment = std::get_if<Session::AttachmentBlock>(&block)) {
            appendAttachment(blocks, *attachment);
        } else if (const auto *image = std::get_if<Session::ImageBlock>(&block)) {
            appendImage(blocks, *image);
        }
    }

    return blocks;
}

void AcpChatBackend::appendAttachment(
    QList<LLMQore::Acp::ContentBlock> &blocks, const Session::AttachmentBlock &attachment) const
{
    const QByteArray content = m_storedContentLoader
                                   ? m_storedContentLoader(m_chatFilePath, attachment.storedPath)
                                   : QByteArray();
    if (content.isEmpty()) {
        LOG_MESSAGE(QString("Could not load attachment %1 for the agent").arg(attachment.fileName));
        blocks.append(
            LLMQore::Acp::ContentBlock::makeText(
                tr("The user attached %1, but it has no readable content.")
                    .arg(attachment.fileName)));
        return;
    }

    QString text = QString::fromUtf8(content.first(qMin(content.size(), maxInlineAttachmentBytes)));
    if (content.size() > maxInlineAttachmentBytes) {
        LOG_MESSAGE(QString("Truncating attachment %1 from %2 to %3 bytes for the agent")
                        .arg(attachment.fileName)
                        .arg(content.size())
                        .arg(maxInlineAttachmentBytes));
        text += QLatin1Char('\n')
                + tr("[truncated by QodeAssist: %1 of %2 bytes shown]")
                      .arg(maxInlineAttachmentBytes)
                      .arg(content.size());
    }

    if (m_agentInfo.agentCapabilities.promptCapabilities.embeddedContext) {
        blocks.append(
            makeResource(
                attachmentUri(attachment.fileName),
                textMimeTypeForFileName(attachment.fileName),
                text));
        return;
    }

    blocks.append(
        LLMQore::Acp::ContentBlock::makeText(
            Session::fencedFileBlock(attachment.fileName, text)));
}

void AcpChatBackend::appendImage(
    QList<LLMQore::Acp::ContentBlock> &blocks, const Session::ImageBlock &image) const
{
    if (!m_agentInfo.agentCapabilities.promptCapabilities.image) {
        blocks.append(
            LLMQore::Acp::ContentBlock::makeText(
                tr("The user attached the image %1, which this agent cannot receive.")
                    .arg(image.fileName)));
        return;
    }

    const QByteArray content = m_storedContentLoader
                                   ? m_storedContentLoader(m_chatFilePath, image.storedPath)
                                   : QByteArray();
    if (content.isEmpty()) {
        LOG_MESSAGE(QString("Could not load image %1 for the agent").arg(image.fileName));
        blocks.append(
            LLMQore::Acp::ContentBlock::makeText(
                tr("The user attached the image %1, but it has no readable content.")
                    .arg(image.fileName)));
        return;
    }

    if (content.size() > maxInlineImageBytes) {
        LOG_MESSAGE(QString("Image %1 is %2 bytes, over the %3 byte limit for an agent prompt")
                        .arg(image.fileName)
                        .arg(content.size())
                        .arg(maxInlineImageBytes));
        blocks.append(
            LLMQore::Acp::ContentBlock::makeText(
                tr("The user attached the image %1, which is too large to send (%2 bytes).")
                    .arg(image.fileName)
                    .arg(content.size())));
        return;
    }

    blocks.append(makeImage(image.mediaType, QString::fromLatin1(content.toBase64())));
}

void AcpChatBackend::sendPrompt()
{
    const QString turnId = m_ledger.activeTurnId();

    m_client->prompt(m_sessionId, buildPrompt(), std::chrono::minutes(30))
        .then(
            this,
            [this, turnId](const LLMQore::Acp::PromptResult &result) {
                if (!m_ledger.isActiveTurn(turnId))
                    return;

                m_handoverSummary.clear();

                const Session::Usage usage = usageFromPromptResult(result.usage);
                if (!usage.isEmpty())
                    emit sessionEvent(
                        Session::UsageReported{.turnId = turnId, .usage = usage});

                m_pendingTurn = {};
                finishTurn(turnId);
                emit sessionEvent(Session::TurnCompleted{.turnId = turnId});
            })
        .onFailed(this, [this, turnId](const std::exception &e) {
            if (!m_ledger.isActiveTurn(turnId))
                return;
            failTurn(QString::fromUtf8(e.what()), false);
        });
}

void AcpChatBackend::connectClient()
{
    connect(
        m_client,
        &LLMQore::Acp::AcpClient::agentMessageChunk,
        this,
        [this](const QString &, const LLMQore::Acp::ContentBlock &content) {
            const QString text = textOf(content);
            if (!m_ledger.hasActiveTurn() || text.isEmpty())
                return;
            emit sessionEvent(Session::TextDelta{.turnId = m_ledger.activeTurnId(), .text = text});
        });

    connect(
        m_client,
        &LLMQore::Acp::AcpClient::agentThoughtChunk,
        this,
        [this](const QString &, const LLMQore::Acp::ContentBlock &content) {
            const QString text = textOf(content);
            if (!m_ledger.hasActiveTurn() || text.isEmpty())
                return;
            emit sessionEvent(
                Session::ThinkingReceived{.turnId = m_ledger.activeTurnId(), .text = text});
        });

    const auto onToolCall
        = [this](const QString &sessionId, const LLMQore::Acp::ToolCall &toolCall) {
              if (!m_ledger.hasActiveTurn() || sessionId != m_sessionId)
                  return;
              emit sessionEvent(
                  Session::ToolCallUpdated{
                      .turnId = m_ledger.activeTurnId(),
                      .toolId = singleLineAgentText(toolCall.toolCallId, maxPermissionTitleLength),
                      .name = singleLineAgentText(toolCall.title, maxPermissionTitleLength),
                      .kind = singleLineAgentText(toolCall.kind, maxPermissionKindLength),
                      .status = singleLineAgentText(toolCall.status, maxPermissionKindLength),
                      .result = toolResultText(toolCall),
                      .details = toolDetailsOf(toolCall),
                      .fromAgent = true});
          };

    connect(m_client, &LLMQore::Acp::AcpClient::toolCallStarted, this, onToolCall);
    connect(m_client, &LLMQore::Acp::AcpClient::toolCallUpdated, this, onToolCall);

    connect(
        m_client,
        &LLMQore::Acp::AcpClient::planUpdated,
        this,
        [this](const QString &, const LLMQore::Acp::Plan &plan) {
            if (!m_ledger.hasActiveTurn())
                return;

            QList<Session::PlanEntry> entries;
            entries.reserve(plan.entries.size());
            for (const LLMQore::Acp::PlanEntry &entry : plan.entries)
                entries.append(Session::PlanEntry{entry.content, entry.priority, entry.status});

            emit sessionEvent(
                Session::PlanUpdated{.turnId = m_ledger.activeTurnId(), .entries = entries});
        });

    connect(
        m_client,
        &LLMQore::Acp::AcpClient::sessionInfoUpdated,
        this,
        [this](const QString &sessionId, const QString &title) {
            const QString suggested = singleLineAgentText(title, maxAgentTitleLength);
            if (suggested.isEmpty())
                return;

            if (sessionId == m_sessionId) {
                emit sessionEvent(Session::SessionInfo{.title = suggested});
                return;
            }

            if (m_establishingSession) {
                m_earlyTitleSessionId = sessionId;
                m_earlyTitle = suggested;
            }
        });

    connect(
        m_client,
        &LLMQore::Acp::AcpClient::availableCommandsUpdated,
        this,
        [this](const QString &sessionId, const QList<LLMQore::Acp::AvailableCommand> &commands) {
            QList<LLMQore::Acp::AvailableCommand> clamped;
            clamped.reserve(qMin(commands.size(), maxAgentCommands));
            for (const LLMQore::Acp::AvailableCommand &command : commands) {
                if (clamped.size() >= maxAgentCommands) {
                    LOG_MESSAGE(
                        QString("Dropping %1 extra agent slash commands")
                            .arg(commands.size() - maxAgentCommands));
                    break;
                }
                static const QRegularExpression commandNamePattern(
                    QStringLiteral("^[A-Za-z0-9._:-]{1,64}$"));
                if (!commandNamePattern.match(command.name).hasMatch()) {
                    LOG_MESSAGE(
                        QString("Skipping agent slash command with an unusable name: %1")
                            .arg(singleLineAgentText(command.name, maxAgentCommandNameLength)));
                    continue;
                }

                LLMQore::Acp::AvailableCommand entry;
                entry.name = command.name;
                entry.description
                    = singleLineAgentText(command.description, maxPermissionTitleLength);
                entry.inputHint = singleLineAgentText(command.inputHint, maxPermissionTitleLength);
                clamped.append(entry);
            }

            if (sessionId == m_sessionId) {
                m_availableCommands = clamped;
                emit availableCommandsChanged();
                return;
            }

            if (m_establishingSession) {
                m_earlyCommandsSessionId = sessionId;
                m_earlyCommands = clamped;
            }
        });

    connect(m_client, &LLMQore::Acp::AcpClient::agentStderr, this, [this](const QString &line) {
        if (m_stderr.size() < maxStderrLines)
            m_stderr.append(line);
    });

    connect(m_client, &LLMQore::Acp::AcpClient::disconnected, this, [this] {
        if (!m_ledger.hasActiveTurn())
            releaseClient();
    });

    connect(m_client, &LLMQore::Acp::AcpClient::errorOccurred, this, [this](const QString &error) {
        if (!m_ledger.hasActiveTurn()) {
            LOG_MESSAGE(QString("Agent reported an error outside a turn: %1").arg(error));
            releaseClient();
            return;
        }
        failTurn(error, true);
    });
}

void AcpChatBackend::requestPermission(
    const QString &requestId,
    const LLMQore::Acp::ToolCall &toolCall,
    const QList<LLMQore::Acp::PermissionOption> &options)
{
    if (!m_ledger.hasActiveTurn()) {
        LOG_MESSAGE(
            QString("Cancelling agent permission request %1 that arrived outside a turn")
                .arg(requestId));
        m_ledger.cancelPermission(requestId);
        return;
    }

    const QString turnId = m_ledger.activeTurnId();
    const QString title = clampAgentText(toolCall.title, maxPermissionTitleLength);

    QList<Session::PermissionOption> offered;
    const QString rejection = validatePermissionOptions(options);

    if (rejection.isEmpty()) {
        offered.reserve(options.size());
        for (const LLMQore::Acp::PermissionOption &option : options) {
            offered.append(
                Session::PermissionOption{
                    option.optionId,
                    clampAgentText(option.name, maxPermissionOptionNameLength),
                    option.kind});
        }
    } else {
        LOG_MESSAGE(
            QString("Declining agent permission request %1 (%2): %3")
                .arg(requestId, title, rejection));
    }

    emit sessionEvent(
        Session::PermissionRequested{
            .turnId = turnId,
            .requestId = requestId,
            .toolCallId = singleLineAgentText(toolCall.toolCallId, maxPermissionTitleLength),
            .title = title,
            .toolKind = clampAgentText(toolCall.kind, maxPermissionKindLength),
            .options = offered});

    if (!rejection.isEmpty()) {
        m_ledger.cancelPermission(requestId);
        emit sessionEvent(
            Session::PermissionResolved{
                .turnId = turnId, .requestId = requestId, .cancelled = true});
    }
}

bool AcpChatBackend::respondPermission(const QString &requestId, const QString &optionId)
{
    if (!m_ledger.resolvePermission(requestId, optionId))
        return false;

    emit sessionEvent(
        Session::PermissionResolved{
            .turnId = m_ledger.activeTurnId(), .requestId = requestId, .optionId = optionId});
    return true;
}

void AcpChatBackend::emitPermissionsCancelled(const QString &turnId, const QStringList &requestIds)
{
    for (const QString &requestId : requestIds) {
        emit sessionEvent(
            Session::PermissionResolved{
                .turnId = turnId, .requestId = requestId, .cancelled = true});
    }
}

void AcpChatBackend::cancelPendingPermissions(const QString &turnId)
{
    emitPermissionsCancelled(turnId, m_ledger.drainPermissions());
}

void AcpChatBackend::finishTurn(const QString &turnId)
{
    emitPermissionsCancelled(turnId, m_ledger.endTurn());
}

void AcpChatBackend::cancel()
{
    const QString turnId = m_ledger.activeTurnId();
    const bool hadTurn = m_ledger.hasActiveTurn();

    m_pendingTurn = {};

    if (hadTurn && !m_sessionId.isEmpty())
        m_handoverSummary.clear();

    finishTurn(turnId);

    if (hadTurn && m_client && !m_sessionId.isEmpty())
        m_client->cancel(m_sessionId);
}

void AcpChatBackend::clearToolSession(const QString &filePath)
{
    Q_UNUSED(filePath)

    cancel();
    releaseClient();
    m_resumeSessionId.clear();
    m_handoverSummary.clear();
}

void AcpChatBackend::failTurn(const QString &error, bool dropProcess)
{
    if (!m_ledger.hasActiveTurn())
        return;

    const QString turnId = m_ledger.activeTurnId();
    m_pendingTurn = {};

    finishTurn(turnId);

    QString details = error;

    const QString hint = m_sessionId.isEmpty() ? runnerHint(m_runner) : QString();
    if (!hint.isEmpty())
        details += QLatin1Char('\n') + hint;

    if (!m_stderr.isEmpty())
        details += QLatin1Char('\n') + tr("Agent output:") + QLatin1Char('\n')
                   + m_stderr.join(QLatin1Char('\n'));

    if (dropProcess)
        releaseClient();

    emit sessionEvent(Session::TurnFailed{.turnId = turnId, .error = details});
}

void AcpChatBackend::releaseClient()
{
    LLMQore::Acp::AcpClient *client = m_client;
    m_client = nullptr;
    ++m_clientGeneration;
    m_establishingSession = false;

    if (!m_sessionId.isEmpty())
        m_resumeSessionId = m_sessionId;

    m_sessionId.clear();
    m_agentInfo = {};
    m_authenticated = false;
    stopKnowledgeServer();

    m_earlyCommands.clear();
    m_earlyCommandsSessionId.clear();
    m_earlyTitle.clear();
    m_earlyTitleSessionId.clear();
    if (!m_availableCommands.isEmpty()) {
        m_availableCommands.clear();
        emit availableCommandsChanged();
    }

    cancelPendingPermissions(m_ledger.activeTurnId());

    if (!client)
        return;

    client->disconnect(this);

    QMetaObject::invokeMethod(
        this,
        [client]() {
            client->shutdown();
            client->deleteLater();
        },
        Qt::QueuedConnection);
}

} // namespace QodeAssist::Acp
