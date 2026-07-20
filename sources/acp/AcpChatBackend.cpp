// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AcpChatBackend.hpp"

#include <chrono>

#include <LLMQore/AcpClient.hpp>
#include <LLMQore/RpcExceptions.hpp>

#include <logger/Logger.hpp>

namespace QodeAssist::Acp {

namespace {

constexpr int authRequiredErrorCode = -32000;
constexpr int maxStderrLines = 20;

QList<LLMQore::Acp::ContentBlock> promptBlocks(const QList<Session::ContentBlock> &userBlocks)
{
    QList<LLMQore::Acp::ContentBlock> blocks;
    for (const Session::ContentBlock &block : userBlocks) {
        if (const auto *text = std::get_if<Session::TextBlock>(&block)) {
            if (!text->text.isEmpty())
                blocks.append(LLMQore::Acp::ContentBlock::makeText(text->text));
        }
    }
    return blocks;
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

    m_pendingPrompt = promptBlocks(request.userBlocks);
    if (m_pendingPrompt.isEmpty()) {
        emit sessionEvent(
            Session::TurnFailed{.turnId = {}, .error = tr("There is nothing to send to the agent.")});
        return;
    }

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

void AcpChatBackend::sendPrompt()
{
    const QString turnId = m_activeTurnId;

    m_client->prompt(m_sessionId, m_pendingPrompt, std::chrono::minutes(30))
        .then(
            this,
            [this, turnId](const LLMQore::Acp::PromptResult &) {
                if (turnId != m_activeTurnId)
                    return;
                m_activeTurnId.clear();
                emit sessionEvent(Session::TurnCompleted{.turnId = turnId});
            })
        .onFailed(this, [this](const std::exception &e) {
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
