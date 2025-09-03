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

#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QUrl>

namespace QodeAssist::LLMCore {

struct HttpRequest
{
    QNetworkRequest networkRequest;
    QString requestId;
    QJsonObject payload;
};

class HttpClient : public QObject
{
    Q_OBJECT

public:
    HttpClient(QObject *parent = nullptr);
    ~HttpClient();

    void cancelRequest(const QString &requestId);

signals:
    void sendRequest(const QodeAssist::LLMCore::HttpRequest &request);
    void dataReceived(const QString &requestId, const QByteArray &data);
    void requestFinished(const QString &requestId, bool success, const QString &error);

private slots:
    void onSendRequest(const QodeAssist::LLMCore::HttpRequest &request);
    void onReadyRead();
    void onFinished();

private:
    QString addActiveRequest(QNetworkReply *reply, const QString &requestId);

    QNetworkAccessManager *m_manager;
    QHash<QString, QNetworkReply *> m_activeRequests;
    mutable QMutex m_mutex;
};

} // namespace QodeAssist::LLMCore
