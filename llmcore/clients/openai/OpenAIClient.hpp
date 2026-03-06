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
#include <QString>
#include <QUrl>

#include <llmcore/core/DataBuffers.hpp>
#include <llmcore/core/RequestType.hpp>
#include <llmcore/network/BaseClient.hpp>
#include <llmcore/network/HttpClient.hpp>

namespace QodeAssist::LLMCore {

class OpenAIMessage;

class OpenAIClient : public BaseClient
{
    Q_OBJECT
public:
    explicit OpenAIClient(QObject *parent = nullptr);
    explicit OpenAIClient(ApiKey apiKey, QObject *parent = nullptr);

    void setApiKey(const QString &apiKey);
    QString apiKey() const;

    void setAuthHeaderName(const QString &name);
    void setAuthHeaderPrefix(const QString &prefix);

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
    void cleanupRequest(const RequestID &requestId);

    ApiKey m_apiKey;
    QString m_authHeaderName = "Authorization";
    QString m_authHeaderPrefix = "Bearer ";
    HttpClient *m_httpClient;

    QHash<RequestID, OpenAIMessage *> m_messages;
    QHash<RequestID, DataBuffers> m_dataBuffers;
    QHash<RequestID, QUrl> m_requestUrls;
    QHash<RequestID, QJsonObject> m_originalRequests;
};

} // namespace QodeAssist::LLMCore
