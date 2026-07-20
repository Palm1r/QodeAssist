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
#include "session/ContentBlock.hpp"
#include "session/TurnContext.hpp"

namespace QodeAssist::Acp {

class AcpChatBackend : public Session::ChatBackend
{
    Q_OBJECT

public:
    using ClientFactory = std::function<
        AgentProcess(const AgentDefinition &agent, const QString &cwd, QObject *parent)>;
    using StoredContentLoader
        = std::function<QByteArray(const QString &chatFilePath, const QString &storedPath)>;

    explicit AcpChatBackend(QObject *parent = nullptr);

    void setClientFactory(ClientFactory factory);
    void setStoredContentLoader(StoredContentLoader loader);

    void bindAgent(const AgentDefinition &agent);
    QString boundAgentId() const;
    QString acpSessionId() const { return m_sessionId; }

    void sendTurn(const Session::TurnRequest &request) override;
    void cancel() override;
    Session::TurnContextNeeds contextNeeds() const override { return {false, false}; }
    void setChatFilePath(const QString &filePath) override;
    void clearToolSession(const QString &filePath) override;

signals:
    void agentSessionStarted(const QString &sessionId);

private:
    struct PendingTurn
    {
        QList<Session::ContentBlock> userBlocks;
        QList<Session::LinkedFile> linkedFiles;
    };

    void startClient();
    void startSession();
    void sendPrompt();
    void authenticateAndRetry();

    QList<LLMQore::Acp::ContentBlock> buildPrompt() const;
    void appendAttachment(
        QList<LLMQore::Acp::ContentBlock> &blocks,
        const Session::AttachmentBlock &attachment) const;
    void appendImage(
        QList<LLMQore::Acp::ContentBlock> &blocks, const Session::ImageBlock &image) const;

    void connectClient();
    void failTurn(const QString &error, bool dropProcess);
    void releaseClient();

    ClientFactory m_clientFactory;
    StoredContentLoader m_storedContentLoader;
    std::optional<AgentDefinition> m_agent;

    LLMQore::Acp::AcpClient *m_client = nullptr;
    LLMQore::Acp::InitializeResult m_agentInfo;
    QString m_sessionId;
    QString m_workingDirectory;
    QString m_runner;

    QString m_chatFilePath;
    QString m_activeTurnId;
    PendingTurn m_pendingTurn;
    QStringList m_stderr;
    int m_turnCounter = 0;
    bool m_authenticated = false;
};

} // namespace QodeAssist::Acp
