/*
 * Copyright (C) 2026 Petr Mironychev
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

#include <functional>
#include <optional>

#include <QFuture>
#include <QHash>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QSet>
#include <QString>
#include <QUrl>

#include <llmcore/core/DataBuffers.hpp>
#include <llmcore/core/RequestType.hpp>
#include <llmcore/network/BaseClient.hpp>
#include <llmcore/network/HttpClient.hpp>

namespace QodeAssist::LLMCore {

class GoogleMessage;

class GoogleAIClient : public BaseClient
{
    Q_OBJECT
public:
    

    explicit GoogleAIClient(QObject *parent = nullptr);
    explicit GoogleAIClient(ApiKey apiKey, QObject *parent = nullptr);

    void setApiKey(const QString &apiKey);
    QString apiKey() const;

    QFuture<QList<QString>> listModels(const QString &baseUrl);

    void sendMessage(
        const RequestID &requestId, const QUrl &url, const QJsonObject &payload);
    void cancelRequest(const RequestID &requestId);

private slots:
    void handleDataReceived(const QString &requestId, const QByteArray &data);
    void handleRequestFinished(const QString &requestId, std::optional<QString> error);
    void handleToolExecutionComplete(
        const QString &requestId, const QHash<QString, QString> &toolResults);

private:
    QNetworkRequest prepareNetworkRequest(const QUrl &url) const;
    void processStreamChunk(const QString &requestId, const QJsonObject &chunk);
    void handleMessageComplete(const QString &requestId);
    void emitPendingThinkingBlocks(const QString &requestId);
    void cleanupRequest(const RequestID &requestId);

    ApiKey m_apiKey;
    HttpClient *m_httpClient;

    QHash<RequestID, GoogleMessage *> m_messages;
    QHash<RequestID, DataBuffers> m_dataBuffers;
    QHash<RequestID, QUrl> m_requestUrls;
    QHash<RequestID, QJsonObject> m_originalRequests;
    QHash<RequestID, int> m_emittedThinkingBlocksCount;
    QSet<RequestID> m_failedRequests;
};

} // namespace QodeAssist::LLMCore
