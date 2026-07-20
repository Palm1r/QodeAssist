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
#include "acp/AgentKnowledgeService.hpp"
#include "acp/AgentSpawn.hpp"
#include "acp/ChatPermissionProvider.hpp"
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
    void setKnowledgeService(AgentKnowledgeService *service);

    void bindAgent(const AgentDefinition &agent);
    QString boundAgentId() const;
    QString acpSessionId() const { return m_sessionId; }
    QString bindingSessionId() const
    {
        return m_sessionId.isEmpty() ? m_resumeSessionId : m_sessionId;
    }

    void resumeSession(const QString &sessionId);
    void startFreshSession();
    void setHandoverSummary(const QString &summary);
    bool isResumePending() const { return !m_resumeSessionId.isEmpty(); }

    void sendTurn(const Session::TurnRequest &request) override;
    void cancel() override;
    bool respondPermission(const QString &requestId, const QString &optionId) override;
    Session::TurnContextNeeds contextNeeds() const override { return {false, false}; }
    void setChatFilePath(const QString &filePath) override;
    void clearToolSession(const QString &filePath) override;

signals:
    void agentSessionStarted(const QString &sessionId);
    void agentTitleSuggested(const QString &title);
    void agentSessionUnavailable(const QString &reason);

private:
    struct PendingTurn
    {
        QList<Session::ContentBlock> userBlocks;
        QList<Session::LinkedFile> linkedFiles;
    };

    void startClient();
    void startSession();
    void resumeOrStartSession();
    void sendPrompt();
    void authenticateAndRetry();

    QList<LLMQore::Acp::ContentBlock> buildPrompt() const;
    void appendAttachment(
        QList<LLMQore::Acp::ContentBlock> &blocks,
        const Session::AttachmentBlock &attachment) const;
    void appendImage(
        QList<LLMQore::Acp::ContentBlock> &blocks, const Session::ImageBlock &image) const;

    void connectClient();
    void requestPermission(
        const QString &requestId,
        const LLMQore::Acp::ToolCall &toolCall,
        const QList<LLMQore::Acp::PermissionOption> &options);
    void cancelPendingPermissions(const QString &turnId);
    void failTurn(const QString &error, bool dropProcess);
    void releaseClient();

    QList<LLMQore::Acp::McpServer> knowledgeServers();
    void stopKnowledgeServer();

    ClientFactory m_clientFactory;
    StoredContentLoader m_storedContentLoader;
    AgentKnowledgeService *m_knowledgeService = nullptr;
    bool m_knowledgeServerRunning = false;
    ChatPermissionProvider *m_permissions = nullptr;
    std::optional<AgentDefinition> m_agent;

    LLMQore::Acp::AcpClient *m_client = nullptr;
    LLMQore::Acp::InitializeResult m_agentInfo;
    QString m_sessionId;
    QString m_workingDirectory;
    QString m_runner;

    QString m_chatFilePath;
    QString m_resumeSessionId;
    QString m_handoverSummary;
    QString m_activeTurnId;
    int m_clientGeneration = 0;
    bool m_establishingSession = false;
    PendingTurn m_pendingTurn;
    QStringList m_stderr;
    int m_turnCounter = 0;
    bool m_authenticated = false;
};

} // namespace QodeAssist::Acp
