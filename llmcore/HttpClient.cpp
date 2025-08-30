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

#include "HttpClient.hpp"

#include <QJsonDocument>
#include <QMutexLocker>
#include <QUuid>

#include <Logger.hpp>

namespace QodeAssist::LLMCore {

HttpClient::HttpClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    connect(this, &HttpClient::sendRequest, this, &HttpClient::onSendRequest);
}

HttpClient::~HttpClient()
{
    QMutexLocker locker(&m_mutex);
    for (auto *reply : std::as_const(m_activeRequests)) {
        reply->abort();
        reply->deleteLater();
    }
    m_activeRequests.clear();
}

void HttpClient::onSendRequest(const HttpRequest &request)
{
    QNetworkRequest networkRequest(request.url);
    networkRequest.setTransferTimeout(300000);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("Accept", "text/event-stream");
    networkRequest.setRawHeader("Cache-Control", "no-cache");
    networkRequest.setRawHeader("Connection", "keep-alive");

    if (request.headers.has_value()) {
        for (const auto &[headername, value] : request.headers->asKeyValueRange()) {
            networkRequest.setRawHeader(headername.toUtf8(), value.toUtf8());
        }
    }

    QJsonDocument doc(request.payload);
    LOG_MESSAGE(QString("HttpClient: Sending POST to %1").arg(request.url.toString()));
    LOG_MESSAGE(QString("HttpClient: data: %1").arg(doc.toJson(QJsonDocument::Indented)));

    QNetworkReply *reply = m_manager->post(networkRequest, doc.toJson(QJsonDocument::Compact));
    addActiveRequest(reply, request.requestId);

    connect(reply, &QNetworkReply::readyRead, this, &HttpClient::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &HttpClient::onFinished);
}

void HttpClient::onReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    QString requestId;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
            if (it.value() == reply) {
                requestId = it.key();
                break;
            }
        }
    }

    if (requestId.isEmpty())
        return;

    QByteArray data = reply->readAll();
    if (!data.isEmpty()) {
        emit dataReceived(requestId, data);
    }
}

void HttpClient::onFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    QString requestId;
    bool hasError = false;
    QString errorMsg;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
            if (it.value() == reply) {
                requestId = it.key();
                m_activeRequests.erase(it);
                break;
            }
        }

        if (reply->error() != QNetworkReply::NoError) {
            hasError = true;
            errorMsg = reply->errorString();
        }
    }

    reply->deleteLater();

    if (!requestId.isEmpty()) {
        emit requestFinished(requestId, !hasError, errorMsg);
    }
}

QString HttpClient::addActiveRequest(QNetworkReply *reply, const QString &requestId)
{
    QMutexLocker locker(&m_mutex);
    m_activeRequests[requestId] = reply;
    LOG_MESSAGE(QString("HttpClient: Added active request: %1").arg(requestId));
    return requestId;
}

void HttpClient::cancelRequest(const QString &requestId)
{
    QMutexLocker locker(&m_mutex);
    if (auto it = m_activeRequests.find(requestId); it != m_activeRequests.end()) {
        it.value()->abort();
        it.value()->deleteLater();
        m_activeRequests.erase(it);
        LOG_MESSAGE(QString("HttpClient: Cancelled request: %1").arg(requestId));
    }
}

} // namespace QodeAssist::LLMCore
