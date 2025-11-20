/* 
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "ChatModel.hpp"
#include "Provider.hpp"
#include "llmcore/IPromptProvider.hpp"
#include <context/ContextManager.hpp>

namespace QodeAssist::Chat {

class ClientInterface : public QObject
{
    Q_OBJECT

public:
    explicit ClientInterface(
        ChatModel *chatModel, LLMCore::IPromptProvider *promptProvider, QObject *parent = nullptr);
    ~ClientInterface();

    void sendMessage(
        const QString &message,
        const QList<QString> &attachments = {},
        const QList<QString> &linkedFiles = {},
        bool useAgentMode = false);
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
    void handleCleanAccumulatedData(const QString &requestId);

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
        LLMCore::Provider *provider;
    };

    LLMCore::IPromptProvider *m_promptProvider = nullptr;
    ChatModel *m_chatModel;
    Context::ContextManager *m_contextManager;
    QString m_chatFilePath;

    QHash<QString, RequestContext> m_activeRequests;
    QHash<QString, QString> m_accumulatedResponses;
};

} // namespace QodeAssist::Chat
