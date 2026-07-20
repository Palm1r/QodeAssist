// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AcpChatBackend.hpp"

#include <chrono>

#include <QMimeDatabase>
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

LLMQore::Acp::ContentBlock makeResourceLink(const Session::LinkedFile &file)
{
    LLMQore::Acp::ContentBlock block;
    block.type = QStringLiteral("resource_link");
    block.uri = QUrl::fromLocalFile(file.path).toString(QUrl::FullyEncoded);
    block.name = file.fileName;
    block.mimeType = mimeTypeForFileName(file.fileName).name();
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

QList<Session::LinkedFile> linkableFiles(const std::optional<Session::TurnContext> &context)
{
    if (!context)
        return {};

    QList<Session::LinkedFile> files;
    for (const Session::LinkedFile &file : context->linkedFiles) {
        if (file.path.isEmpty())
            LOG_MESSAGE(QString("Linked file %1 has no path to link to").arg(file.fileName));
        else
            files.append(file);
    }
    return files;
}

bool hasSendableContent(
    const QList<Session::ContentBlock> &userBlocks, const QList<Session::LinkedFile> &linkedFiles)
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
    return !linkedFiles.isEmpty();
}

QString textOf(const LLMQore::Acp::ContentBlock &block)
{
    return block.type == QLatin1String("text") ? block.text : QString();
}

} // namespace

AcpChatBackend::AcpChatBackend(QObject *parent)
    : Session::ChatBackend(parent)
    , m_clientFactory(&spawnAgent)
{}

void AcpChatBackend::setClientFactory(ClientFactory factory)
{
    m_clientFactory = std::move(factory);
}

void AcpChatBackend::setStoredContentLoader(StoredContentLoader loader)
{
    m_storedContentLoader = std::move(loader);
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
    m_authenticated = false;
}

QString AcpChatBackend::boundAgentId() const
{
    return m_agent ? m_agent->id : QString();
}

void AcpChatBackend::sendTurn(const Session::TurnRequest &request)
{
    if (!m_agent) {
        emit sessionEvent(
            Session::TurnFailed{.turnId = {}, .error = tr("No agent is bound to this chat.")});
        return;
    }

    cancel();

    QList<Session::LinkedFile> linkedFiles = linkableFiles(request.context);
    if (!hasSendableContent(request.userBlocks, linkedFiles)) {
        emit sessionEvent(
            Session::TurnFailed{.turnId = {}, .error = tr("There is nothing to send to the agent.")});
        return;
    }

    m_pendingTurn = PendingTurn{request.userBlocks, std::move(linkedFiles)};

    m_activeTurnId = QStringLiteral("acp-%1").arg(++m_turnCounter);
    m_stderr.clear();
    emit sessionEvent(Session::TurnStarted{.turnId = m_activeTurnId});

    if (!m_client) {
        startClient();
        return;
    }

    if (m_sessionId.isEmpty()) {
        startSession();
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

    connectClient();

    m_client->connectAndInitialize(std::chrono::seconds(60))
        .then(
            this,
            [this](const LLMQore::Acp::InitializeResult &result) {
                m_agentInfo = result;
                if (!m_activeTurnId.isEmpty())
                    startSession();
            })
        .onFailed(this, [this](const std::exception &e) {
            failTurn(QString::fromUtf8(e.what()), true);
        });
}

void AcpChatBackend::startSession()
{
    LLMQore::Acp::NewSessionParams params;
    params.cwd = m_workingDirectory;

    m_client->newSession(params, std::chrono::seconds(60))
        .then(
            this,
            [this](const LLMQore::Acp::NewSessionResult &result) {
                m_sessionId = result.sessionId;
                emit agentSessionStarted(m_sessionId);
                if (!m_activeTurnId.isEmpty())
                    sendPrompt();
            })
        .onFailed(
            this,
            [this](const LLMQore::Rpc::RemoteError &error) {
                const bool needsAuthentication = error.code() == authRequiredErrorCode
                                                 && !m_authenticated
                                                 && !m_agentInfo.authMethods.isEmpty();
                if (needsAuthentication)
                    authenticateAndRetry();
                else
                    failTurn(error.remoteMessage(), true);
            })
        .onFailed(this, [this](const std::exception &e) {
            failTurn(QString::fromUtf8(e.what()), true);
        });
}

void AcpChatBackend::authenticateAndRetry()
{
    const LLMQore::Acp::AuthMethod method = m_agentInfo.authMethods.first();
    LOG_MESSAGE(QString("ACP agent requires authentication, using method: %1").arg(method.id));

    m_authenticated = true;

    m_client->authenticate(method.id, std::chrono::minutes(5))
        .then(
            this,
            [this]() {
                if (!m_activeTurnId.isEmpty())
                    startSession();
            })
        .onFailed(this, [this, method](const std::exception &e) {
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

    for (const Session::LinkedFile &file : m_pendingTurn.linkedFiles)
        blocks.append(makeResourceLink(file));

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
    const QString turnId = m_activeTurnId;

    m_client->prompt(m_sessionId, buildPrompt(), std::chrono::minutes(30))
        .then(
            this,
            [this, turnId](const LLMQore::Acp::PromptResult &) {
                if (turnId != m_activeTurnId)
                    return;
                m_activeTurnId.clear();
                m_pendingTurn = {};
                emit sessionEvent(Session::TurnCompleted{.turnId = turnId});
            })
        .onFailed(this, [this, turnId](const std::exception &e) {
            if (turnId != m_activeTurnId)
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
            if (m_activeTurnId.isEmpty() || text.isEmpty())
                return;
            emit sessionEvent(Session::TextDelta{.turnId = m_activeTurnId, .text = text});
        });

    connect(
        m_client,
        &LLMQore::Acp::AcpClient::agentThoughtChunk,
        this,
        [this](const QString &, const LLMQore::Acp::ContentBlock &content) {
            const QString text = textOf(content);
            if (m_activeTurnId.isEmpty() || text.isEmpty())
                return;
            emit sessionEvent(Session::ThinkingReceived{.turnId = m_activeTurnId, .text = text});
        });

    connect(m_client, &LLMQore::Acp::AcpClient::agentStderr, this, [this](const QString &line) {
        if (m_stderr.size() < maxStderrLines)
            m_stderr.append(line);
    });

    connect(m_client, &LLMQore::Acp::AcpClient::errorOccurred, this, [this](const QString &error) {
        failTurn(error, true);
    });
}

void AcpChatBackend::cancel()
{
    if (m_activeTurnId.isEmpty())
        return;

    m_activeTurnId.clear();
    m_pendingTurn = {};

    if (m_client && !m_sessionId.isEmpty())
        m_client->cancel(m_sessionId);
}

void AcpChatBackend::clearToolSession(const QString &filePath)
{
    Q_UNUSED(filePath)

    cancel();
    releaseClient();
}

void AcpChatBackend::failTurn(const QString &error, bool dropProcess)
{
    if (m_activeTurnId.isEmpty())
        return;

    const QString turnId = m_activeTurnId;
    m_activeTurnId.clear();
    m_pendingTurn = {};

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
    m_sessionId.clear();
    m_agentInfo = {};
    m_authenticated = false;

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
