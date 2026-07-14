// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include "ChatModel.hpp"
#include "providers/Provider.hpp"
#include "templates/IPromptProvider.hpp"
#include <LLMQore/BaseClient.hpp>
#include <context/ContextManager.hpp>

namespace QodeAssist::Skills {
class SkillsManager;
}

namespace QodeAssist::Chat {

class ClientInterface : public QObject
{
    Q_OBJECT

public:
    explicit ClientInterface(
        ChatModel *chatModel, Templates::IPromptProvider *promptProvider, QObject *parent = nullptr);
    ~ClientInterface();

    void setSkillsManager(Skills::SkillsManager *skillsManager);

    void sendMessage(
        const QString &message,
        const QList<QString> &attachments = {},
        const QList<QString> &linkedFiles = {},
        bool useTools = false,
        bool useThinking = false);
    void clearMessages();
    void cancelRequest();

    Context::ContextManager *contextManager() const;
    
    void setChatFilePath(const QString &filePath);
    QString chatFilePath() const;

signals:
    void errorOccurred(const QString &error);
    void messageReceivedCompletely();
    void requestStarted(const QString &requestId);
    void messageUsageReceived(
        int promptTokens, int completionTokens, int cachedPromptTokens, int reasoningTokens);

private slots:
    void handlePartialResponse(const QString &requestId, const QString &partialText);
    void handleFullResponse(const QString &requestId, const QString &fullText);
    void handleRequestFinalized(const ::LLMQore::RequestID &requestId, const ::LLMQore::CompletionInfo &info);
    void handleRequestFailed(const QString &requestId, const QString &error);
    void handleThinkingBlockReceived(
        const QString &requestId, const QString &thinking, const QString &signature);
    void handleToolExecutionStarted(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QJsonObject &arguments);
    void handleToolExecutionCompleted(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QString &toolOutput);

private:
    void handleLLMResponse(const QString &response, const QJsonObject &request);
    QString getCurrentFileContext() const;
    QString getSystemPromptWithLinkedFiles(
        const QString &basePrompt, const QList<QString> &linkedFiles) const;
    bool isImageFile(const QString &filePath) const;
    QString getMediaTypeForImage(const QString &filePath) const;
    QString encodeImageToBase64(const QString &filePath) const;
    QVector<LLMCore::ImageAttachment> loadImagesFromStorage(const QList<ChatModel::ImageAttachment> &storedImages) const;

    struct RequestContext
    {
        QJsonObject originalRequest;
        Providers::Provider *provider;
        bool dropPreToolText = false;
    };

    Templates::IPromptProvider *m_promptProvider = nullptr;
    ChatModel *m_chatModel;
    Context::ContextManager *m_contextManager;
    Skills::SkillsManager *m_skillsManager = nullptr;
    QString m_chatFilePath;

    QHash<QString, RequestContext> m_activeRequests;
    QHash<QString, QString> m_accumulatedResponses;
    QSet<QString> m_awaitingContinuation;
};

} // namespace QodeAssist::Chat
