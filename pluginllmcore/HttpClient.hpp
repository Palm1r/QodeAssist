// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

namespace QodeAssist::PluginLLMCore {

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

} // namespace QodeAssist::PluginLLMCore
