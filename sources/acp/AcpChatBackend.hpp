// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>
#include <optional>

#include <QString>
#include <QStringList>

#include <LLMQore/AcpTypes.hpp>

#include "acp/AgentDefinition.hpp"
#include "acp/AgentSpawn.hpp"
#include "session/ChatBackend.hpp"

namespace QodeAssist::Acp {

class AcpChatBackend : public Session::ChatBackend
{
    Q_OBJECT

public:
    using ClientFactory = std::function<
        AgentProcess(const AgentDefinition &agent, const QString &cwd, QObject *parent)>;

    explicit AcpChatBackend(QObject *parent = nullptr);

    void setClientFactory(ClientFactory factory);

    void bindAgent(const AgentDefinition &agent);
    QString boundAgentId() const;
    QString acpSessionId() const { return m_sessionId; }

    void sendTurn(const Session::TurnRequest &request) override;
    void cancel() override;
    void clearToolSession(const QString &filePath) override;

signals:
    void agentSessionStarted(const QString &sessionId);

private:
    void startClient();
    void startSession();
    void sendPrompt();
    void authenticateAndRetry();

    void connectClient();
    void failTurn(const QString &error, bool dropProcess);
    void releaseClient();

    ClientFactory m_clientFactory;
    std::optional<AgentDefinition> m_agent;

    LLMQore::Acp::AcpClient *m_client = nullptr;
    LLMQore::Acp::InitializeResult m_agentInfo;
    QString m_sessionId;
    QString m_workingDirectory;
    QString m_runner;

    QString m_activeTurnId;
    QList<LLMQore::Acp::ContentBlock> m_pendingPrompt;
    QStringList m_stderr;
    int m_turnCounter = 0;
    bool m_authenticated = false;
};

} // namespace QodeAssist::Acp
