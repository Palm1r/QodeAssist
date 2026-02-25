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

#include <Logger.hpp>

namespace QodeAssist::LLMCore {

HttpClient::HttpClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{}

HttpClient::~HttpClient()
{
    QMutexLocker locker(&m_mutex);
    for (auto *reply : std::as_const(m_activeRequests)) {
        reply->abort();
        reply->deleteLater();
    }
    m_activeRequests.clear();
}

QFuture<QByteArray> HttpClient::get(const QNetworkRequest &request)
{
    LOG_MESSAGE(QString("HttpClient: GET %1").arg(request.url().toString()));

    auto promise = std::make_shared<QPromise<QByteArray>>();
    promise->start();

    QNetworkReply *reply = m_manager->get(request);
    setupNonStreamingReply(reply, promise);

    return promise->future();
}

QFuture<QByteArray> HttpClient::post(const QNetworkRequest &request, const QJsonObject &payload)
{
    QJsonDocument doc(payload);
    LOG_MESSAGE(QString("HttpClient: POST %1, data: %2")
                    .arg(request.url().toString(), doc.toJson(QJsonDocument::Indented)));

    auto promise = std::make_shared<QPromise<QByteArray>>();
    promise->start();

    QNetworkReply *reply = m_manager->post(request, doc.toJson(QJsonDocument::Compact));
    setupNonStreamingReply(reply, promise);

    return promise->future();
}

QFuture<QByteArray> HttpClient::del(const QNetworkRequest &request,
                                    std::optional<QJsonObject> payload)
{
    auto promise = std::make_shared<QPromise<QByteArray>>();
    promise->start();

    QNetworkReply *reply;
    if (payload) {
        QJsonDocument doc(*payload);
        LOG_MESSAGE(QString("HttpClient: DELETE %1, data: %2")
                        .arg(request.url().toString(), doc.toJson(QJsonDocument::Indented)));
        reply = m_manager->sendCustomRequest(request, "DELETE", doc.toJson(QJsonDocument::Compact));
    } else {
        LOG_MESSAGE(QString("HttpClient: DELETE %1").arg(request.url().toString()));
        reply = m_manager->deleteResource(request);
    }

    setupNonStreamingReply(reply, promise);

    return promise->future();
}

void HttpClient::setupNonStreamingReply(QNetworkReply *reply,
                                        std::shared_ptr<QPromise<QByteArray>> promise)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, promise]() {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray responseBody = reply->readAll();
        QNetworkReply::NetworkError networkError = reply->error();
        QString networkErrorString = reply->errorString();

        reply->disconnect();
        reply->deleteLater();

        LOG_MESSAGE(
            QString("HttpClient: Non-streaming request - HTTP Status: %1").arg(statusCode));

        bool hasError = (networkError != QNetworkReply::NoError) || (statusCode >= 400);
        if (hasError) {
            QString errorMsg = parseErrorFromResponse(statusCode, responseBody, networkErrorString);
            LOG_MESSAGE(QString("HttpClient: Non-streaming request - Error: %1").arg(errorMsg));
            promise->setException(
                std::make_exception_ptr(std::runtime_error(errorMsg.toStdString())));
        } else {
            promise->addResult(responseBody);
        }
        promise->finish();
    });
}

void HttpClient::postStreaming(const QString &requestId, const QNetworkRequest &request,
                               const QJsonObject &payload)
{
    QJsonDocument doc(payload);
    LOG_MESSAGE(QString("HttpClient: POST streaming %1, data: %2")
                    .arg(request.url().toString(), doc.toJson(QJsonDocument::Indented)));

    QNetworkReply *reply = m_manager->post(request, doc.toJson(QJsonDocument::Compact));
    addActiveRequest(reply, requestId);

    connect(reply, &QNetworkReply::readyRead, this, &HttpClient::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &HttpClient::onStreamingFinished);
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

void HttpClient::onReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply->isFinished())
        return;

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode >= 400)
        return;

    QString requestId = findRequestId(reply);
    if (requestId.isEmpty())
        return;

    QByteArray data = reply->readAll();
    if (!data.isEmpty()) {
        emit dataReceived(requestId, data);
    }
}

void HttpClient::onStreamingFinished()
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
    std::optional<QString> error;

    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
            if (it.value() == reply) {
                requestId = it.key();
                m_activeRequests.erase(it);
                break;
            }
        }

        if (requestId.isEmpty()) {
            reply->deleteLater();
            return;
        }

        bool hasError = (networkError != QNetworkReply::NoError) || (statusCode >= 400);
        if (hasError) {
            error = parseErrorFromResponse(statusCode, responseBody, networkErrorString);
        }

        LOG_MESSAGE(
            QString("HttpClient: Request %1 - HTTP Status: %2").arg(requestId).arg(statusCode));

        if (!responseBody.isEmpty()) {
            LOG_MESSAGE(QString("HttpClient: Request %1 - Response body (%2 bytes): %3")
                            .arg(requestId)
                            .arg(responseBody.size())
                            .arg(QString::fromUtf8(responseBody)));
        }

        if (error) {
            LOG_MESSAGE(QString("HttpClient: Request %1 - Error: %2").arg(requestId, *error));
        }
    }

    reply->deleteLater();

    if (!requestId.isEmpty()) {
        emit requestFinished(requestId, error);
    }
}

QString HttpClient::findRequestId(QNetworkReply *reply)
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
        if (it.value() == reply)
            return it.key();
    }
    return {};
}

void HttpClient::addActiveRequest(QNetworkReply *reply, const QString &requestId)
{
    QMutexLocker locker(&m_mutex);
    m_activeRequests[requestId] = reply;
    LOG_MESSAGE(QString("HttpClient: Added active request: %1").arg(requestId));
}

QString HttpClient::parseErrorFromResponse(
    int statusCode, const QByteArray &responseBody, const QString &networkErrorString)
{
    if (!responseBody.isEmpty()) {
        QJsonDocument errorDoc = QJsonDocument::fromJson(responseBody);
        if (!errorDoc.isNull() && errorDoc.isObject()) {
            QJsonObject errorObj = errorDoc.object();
            if (errorObj.contains("error")) {
                QJsonObject error = errorObj["error"].toObject();
                QString message = error["message"].toString();
                QString type = error["type"].toString();
                QString code = error["code"].toString();

                QString errorMsg = QString("HTTP %1: %2").arg(statusCode).arg(message);
                if (!type.isEmpty())
                    errorMsg += QString(" (type: %1)").arg(type);
                if (!code.isEmpty())
                    errorMsg += QString(" (code: %1)").arg(code);
                return errorMsg;
            }
            return QString("HTTP %1: %2")
                .arg(statusCode)
                .arg(QString::fromUtf8(responseBody));
        }
        return QString("HTTP %1: %2").arg(statusCode).arg(QString::fromUtf8(responseBody));
    }
    return QString("HTTP %1: %2").arg(statusCode).arg(networkErrorString);
}

} // namespace QodeAssist::LLMCore
