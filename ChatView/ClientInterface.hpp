// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include "ChatModel.hpp"
#include "Provider.hpp"
#include "pluginllmcore/IPromptProvider.hpp"
#include <context/ContextManager.hpp>

namespace QodeAssist::Chat {

class ClientInterface : public QObject
{
    Q_OBJECT

public:
    explicit ClientInterface(
        ChatModel *chatModel, PluginLLMCore::IPromptProvider *promptProvider, QObject *parent = nullptr);
    ~ClientInterface();

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

private slots:
    void handlePartialResponse(const QString &requestId, const QString &partialText);
    void handleFullResponse(const QString &requestId, const QString &fullText);
    void handleRequestFailed(const QString &requestId, const QString &error);
    void handleThinkingBlockReceived(
        const QString &requestId, const QString &thinking, const QString &signature);
    void handleToolExecutionStarted(
        const QString &requestId, const QString &toolId, const QString &toolName);
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
    QVector<PluginLLMCore::ImageAttachment> loadImagesFromStorage(const QList<ChatModel::ImageAttachment> &storedImages) const;

    struct RequestContext
    {
        QJsonObject originalRequest;
        PluginLLMCore::Provider *provider;
    };

    PluginLLMCore::IPromptProvider *m_promptProvider = nullptr;
    ChatModel *m_chatModel;
    Context::ContextManager *m_contextManager;
    QString m_chatFilePath;

    QHash<QString, RequestContext> m_activeRequests;
    QHash<QString, QString> m_accumulatedResponses;
    QSet<QString> m_awaitingContinuation;
};

} // namespace QodeAssist::Chat
