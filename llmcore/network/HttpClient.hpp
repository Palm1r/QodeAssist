/*
 * Copyright (C) 2025 Petr Mironychev
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

#include <optional>

#include <QFuture>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPromise>

namespace QodeAssist::LLMCore {

class HttpClient : public QObject
{
    Q_OBJECT

public:
    HttpClient(QObject *parent = nullptr);
    ~HttpClient();

    // Non-streaming — return QFuture with full response
    QFuture<QByteArray> get(const QNetworkRequest &request);
    QFuture<QByteArray> post(const QNetworkRequest &request, const QJsonObject &payload);
    QFuture<QByteArray> del(const QNetworkRequest &request,
                            std::optional<QJsonObject> payload = std::nullopt);

    // Streaming — signal-based with requestId
    void postStreaming(const QString &requestId, const QNetworkRequest &request,
                       const QJsonObject &payload);

    void cancelRequest(const QString &requestId);

signals:
    void dataReceived(const QString &requestId, const QByteArray &data);
    void requestFinished(const QString &requestId, std::optional<QString> error);

private slots:
    void onReadyRead();
    void onStreamingFinished();

private:
    void setupNonStreamingReply(QNetworkReply *reply, std::shared_ptr<QPromise<QByteArray>> promise);

    QString findRequestId(QNetworkReply *reply);
    void addActiveRequest(QNetworkReply *reply, const QString &requestId);
    QString parseErrorFromResponse(int statusCode, const QByteArray &responseBody,
                                   const QString &networkErrorString);

    QNetworkAccessManager *m_manager;
    QHash<QString, QNetworkReply *> m_activeRequests;
    mutable QMutex m_mutex;
};

} // namespace QodeAssist::LLMCore
