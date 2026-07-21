// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QString>

#include <LLMQore/AcpTypes.hpp>

#include "ChatModel.hpp"
#include "ConversationPorts.hpp"
#include "acp/AgentBinding.hpp"
#include "acp/AgentDefinition.hpp"
#include "session/Session.hpp"
#include "templates/IPromptProvider.hpp"
#include <context/ContextManager.hpp>

namespace QodeAssist::Skills {
class SkillsManager;
}

namespace QodeAssist::Acp {
class AcpChatBackend;
}

namespace QodeAssist::Mcp {
class AgentKnowledgeServer;
}

namespace QodeAssist::Chat {

class LlmChatBackend;

class ChatController : public QObject, public IConversationPort
{
    Q_OBJECT

public:
    explicit ChatController(
        ChatModel *chatModel, Templates::IPromptProvider *promptProvider, QObject *parent = nullptr);

    void setSkillsManager(Skills::SkillsManager *skillsManager);

    void sendMessage(const QString &message, const QList<QString> &attachments = {});
    void cancelRequest();
    void resetToRow(int rowIndex);
    void respondToPermission(const QString &requestId, const QString &optionId);

    Session::Session *session() const;
    Context::ContextManager *contextManager() const;

    void setChatFilePath(const QString &filePath);
    QString chatFilePath() const;

    QString boundAgentId() const override;
    QString boundAgentName() const;
    QList<LLMQore::Acp::AvailableCommand> agentCommands() const;
    bool conversationStarted() const override;
    bool transcriptEmpty() const override;
    Acp::AgentBinding agentBinding() const override;

    void bindAgent(const Acp::AgentDefinition &agent) override;
    void bindLlm() override;
    void clearConversation() override;

    void resumeAgentSession(const QString &sessionId) override;
    void startFreshAgentSession() override;
    void startFreshAgentSession(const QString &handoverSummary) override;
    void releaseAgentSession() override;

signals:
    void sessionInfoReceived(const QString &title);
    void agentCommandsChanged();
    void agentSessionUnavailable(const QString &reason);
    void errorOccurred(const QString &error);
    void messageReceivedCompletely();
    void requestStarted(const QString &requestId);
    void messageUsageReceived(
        int promptTokens, int completionTokens, int cachedPromptTokens, int reasoningTokens);

private:
    QList<Session::ContentBlock> composeUserBlocks(
        const QString &message, const QList<QString> &attachments);
    Session::TurnContext buildTurnContext(const QString &message) const;
    void recordFileEditStatus(
        const QString &editId, const QString &status, const QString &fallbackMessage);
    void registerHistoricalEdits();
    void activateBackend(Session::ChatBackend *backend);

    bool isImageFile(const QString &filePath) const;
    QString getMediaTypeForImage(const QString &filePath) const;
    QString encodeImageToBase64(const QString &filePath) const;

    Templates::IPromptProvider *m_promptProvider = nullptr;
    ChatModel *m_chatModel = nullptr;
    Context::ContextManager *m_contextManager = nullptr;
    Skills::SkillsManager *m_skillsManager = nullptr;
    Session::Session *m_session = nullptr;
    LlmChatBackend *m_llmBackend = nullptr;
    Acp::AcpChatBackend *m_acpBackend = nullptr;
    Mcp::AgentKnowledgeServer *m_agentKnowledge = nullptr;
    Session::ChatBackend *m_backend = nullptr;
    QString m_chatFilePath;
};

} // namespace QodeAssist::Chat
