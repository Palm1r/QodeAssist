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
    QJsonDocument doc(request.payload);
    LOG_MESSAGE(QString("HttpClient: data: %1").arg(doc.toJson(QJsonDocument::Indented)));

    QNetworkReply *reply
        = m_manager->post(request.networkRequest, doc.toJson(QJsonDocument::Compact));
    addActiveRequest(reply, request.requestId);

    connect(reply, &QNetworkReply::readyRead, this, &HttpClient::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &HttpClient::onFinished);
}

void HttpClient::onReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    if (!reply || reply->isFinished())
        return;

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode >= 400) {
        return;
    }

    QString requestId;
    {
        QMutexLocker locker(&m_mutex);
        bool found = false;
        for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
            if (it.value() == reply) {
                requestId = it.key();
                found = true;
                break;
            }
        }

        if (!found)
            return;
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

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseBody = reply->readAll();
    QNetworkReply::NetworkError networkError = reply->error();
    QString networkErrorString = reply->errorString();

    reply->disconnect();

    QString requestId;
    bool hasError = false;
    QString errorMsg;

    {
        QMutexLocker locker(&m_mutex);
        bool found = false;
        for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
            if (it.value() == reply) {
                requestId = it.key();
                m_activeRequests.erase(it);
                found = true;
                break;
            }
        }

        if (!found) {
            reply->deleteLater();
            return;
        }

        hasError = (networkError != QNetworkReply::NoError) || (statusCode >= 400);

        if (hasError) {
            errorMsg = parseErrorFromResponse(statusCode, responseBody, networkErrorString);
        }

        LOG_MESSAGE(QString("HttpClient: Request %1 - HTTP Status: %2").arg(requestId).arg(statusCode));

        if (!responseBody.isEmpty()) {
            LOG_MESSAGE(QString("HttpClient: Request %1 - Response body (%2 bytes): %3")
                            .arg(requestId)
                            .arg(responseBody.size())
                            .arg(QString::fromUtf8(responseBody)));
        }

        if (hasError) {
            LOG_MESSAGE(QString("HttpClient: Request %1 - Error: %2").arg(requestId, errorMsg));
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

QString HttpClient::parseErrorFromResponse(
    int statusCode, const QByteArray &responseBody, const QString &networkErrorString)
{
    QString errorMsg;

    if (!responseBody.isEmpty()) {
        QJsonDocument errorDoc = QJsonDocument::fromJson(responseBody);
        if (!errorDoc.isNull() && errorDoc.isObject()) {
            QJsonObject errorObj = errorDoc.object();
            if (errorObj.contains("error")) {
                QJsonObject error = errorObj["error"].toObject();
                QString message = error["message"].toString();
                QString type = error["type"].toString();
                QString code = error["code"].toString();

                errorMsg = QString("HTTP %1: %2").arg(statusCode).arg(message);
                if (!type.isEmpty())
                    errorMsg += QString(" (type: %1)").arg(type);
                if (!code.isEmpty())
                    errorMsg += QString(" (code: %1)").arg(code);
            } else {
                errorMsg = QString("HTTP %1: %2").arg(statusCode).arg(QString::fromUtf8(responseBody));
            }
        } else {
            errorMsg = QString("HTTP %1: %2").arg(statusCode).arg(QString::fromUtf8(responseBody));
        }
    } else {
        errorMsg = QString("HTTP %1: %2").arg(statusCode).arg(networkErrorString);
    }

    return errorMsg;
}

void HttpClient::cancelRequest(const QString &requestId)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_activeRequests.find(requestId);
    if (it != m_activeRequests.end()) {
        QNetworkReply *reply = it.value();
        if (reply) {
            reply->disconnect();
            reply->abort();
            reply->deleteLater();
        }
        m_activeRequests.erase(it);
        LOG_MESSAGE(QString("HttpClient: Cancelled request: %1").arg(requestId));
    }
}

} // namespace QodeAssist::LLMCore
