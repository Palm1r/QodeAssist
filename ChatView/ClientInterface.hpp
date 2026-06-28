// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <memory>

#include "ChatModel.hpp"
#include "ChatSerializer.hpp"
#include <ErrorInfo.hpp>
#include <LLMQore/BaseClient.hpp>
#include <ResponseEvent.hpp>
#include <context/ContextManager.hpp>

namespace QodeAssist {
class SessionManager;
class Session;
class ConversationHistory;
}

namespace QodeAssist::Skills {
class SkillsManager;
}

namespace QodeAssist::Chat {

class ClientInterface : public QObject
{
    Q_OBJECT

public:
    explicit ClientInterface(ChatModel *chatModel, QObject *parent = nullptr);
    ~ClientInterface();

    void setSkillsManager(Skills::SkillsManager *skillsManager);
    void setSessionManager(SessionManager *sessionManager);
    void setHistory(ConversationHistory *history);
    void setActiveAgent(const QString &agentName);

    void sendMessage(
        const QString &message,
        const QList<QString> &attachments = {},
        const QList<QString> &linkedFiles = {});
    void clearMessages();
    void cancelRequest();

    Context::ContextManager *contextManager() const;

    void setChatFilePath(const QString &filePath);
    QString chatFilePath() const;

    void ensureSession();
    Session *session() const;

signals:
    void errorOccurred(const QString &error);
    void messageReceivedCompletely();
    void requestStarted(const QString &requestId);
    void messageUsageReceived(
        int promptTokens, int completionTokens, int cachedPromptTokens, int reasoningTokens);

private:
    bool ensureAgentBound();

    void onSessionEvent(Session *session, const QodeAssist::ResponseEvent &ev);
    void onSessionFinished(const QString &requestId);
    void onSessionFailed(const QString &requestId, const QodeAssist::ErrorInfo &error);

    QStringList invokedSkillNames(const QString &message) const;
    QString buildChatContextLayer() const;
    QString requestIdForSession(Session *session) const;
    bool isImageFile(const QString &filePath) const;
    QString getMediaTypeForImage(const QString &filePath) const;
    QString encodeImageToBase64(const QString &filePath) const;

    struct RequestContext
    {
        QJsonObject originalRequest;
        QPointer<Session> session;
    };

    ChatModel *m_chatModel;
    Context::ContextManager *m_contextManager;
    QPointer<ConversationHistory> m_history;
    Skills::SkillsManager *m_skillsManager = nullptr;
    QPointer<SessionManager> m_sessionManager;
    QPointer<Session> m_session;
    QString m_activeAgent;
    QString m_boundAgent;
    QString m_chatFilePath;
    std::shared_ptr<StoredContentCache> m_contentCache;

    QHash<QString, RequestContext> m_activeRequests;
};

} // namespace QodeAssist::Chat
