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

#include "RequestHandler.hpp"
#include "Logger.hpp"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QThread>

namespace QodeAssist::LLMCore {

RequestHandler::RequestHandler(QObject *parent)
    : RequestHandlerBase(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    connect(
        this,
        &RequestHandler::doSendRequest,
        this,
        &RequestHandler::sendLLMRequestInternal,
        Qt::QueuedConnection);

    connect(
        this,
        &RequestHandler::doCancelRequest,
        this,
        &RequestHandler::cancelRequestInternal,
        Qt::QueuedConnection);
}

RequestHandler::~RequestHandler()
{
    for (auto reply : m_activeRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_activeRequests.clear();
    m_accumulatedResponses.clear();
}

void RequestHandler::sendLLMRequest(const LLMConfig &config, const QJsonObject &request)
{
    emit doSendRequest(config, request);
}

bool RequestHandler::cancelRequest(const QString &id)
{
    emit doCancelRequest(id);
    return true;
}

void RequestHandler::sendLLMRequestInternal(const LLMConfig &config, const QJsonObject &request)
{
    LOG_MESSAGE(QString("Sending request to llm: \nurl: %1\nRequest body:\n%2")
                    .arg(
                        config.url.toString(),
                        QString::fromUtf8(
                            QJsonDocument(config.providerRequest).toJson(QJsonDocument::Indented))));

    QNetworkRequest networkRequest(config.url);
    networkRequest.setTransferTimeout(300000);

    config.provider->prepareNetworkRequest(networkRequest);

    QNetworkReply *reply
        = m_manager->post(networkRequest, QJsonDocument(config.providerRequest).toJson());
    if (!reply) {
        LOG_MESSAGE("Error: Failed to create network reply");
        return;
    }

    QString requestId = request["id"].toString();
    m_activeRequests[requestId] = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, request, config]() {
        handleLLMResponse(reply, request, config);
    });

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, requestId]() {
            m_activeRequests.remove(requestId);
            if (reply->error() != QNetworkReply::NoError) {
                QString errorMessage = reply->errorString();
                int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

                LOG_MESSAGE(
                    QString("Error details: %1\nStatus code: %2").arg(errorMessage).arg(statusCode));

                emit requestFinished(requestId, false, errorMessage);
            } else {
                LOG_MESSAGE("Request finished successfully");
                emit requestFinished(requestId, true, QString());
            }

            reply->deleteLater();
        },
        Qt::QueuedConnection);
}

void RequestHandler::handleLLMResponse(
    QNetworkReply *reply, const QJsonObject &request, const LLMConfig &config)
{
    QString &accumulatedResponse = m_accumulatedResponses[reply];

    bool isComplete = config.provider->handleResponse(reply, accumulatedResponse);

    if (config.requestType == RequestType::CodeCompletion) {
        if (!config.multiLineCompletion
            && processSingleLineCompletion(reply, request, accumulatedResponse, config)) {
            return;
        }

        if (isComplete) {
            auto cleanedCompletion
                = removeStopWords(accumulatedResponse, config.promptTemplate->stopWords());

            emit completionReceived(cleanedCompletion, request, true);
        }
    } else if (config.requestType == RequestType::Chat) {
        emit completionReceived(accumulatedResponse, request, isComplete);
    }

    if (isComplete)
        m_accumulatedResponses.remove(reply);
}

void RequestHandler::cancelRequestInternal(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    if (m_activeRequests.contains(id)) {
        QNetworkReply *reply = m_activeRequests[id];

        disconnect(reply, nullptr, this, nullptr);

        reply->abort();
        m_activeRequests.remove(id);
        m_accumulatedResponses.remove(reply);

        reply->deleteLater();

        locker.unlock();

        m_manager->clearConnectionCache();
        m_manager->clearAccessCache();

        emit requestCancelled(id);
    }
}

bool RequestHandler::processSingleLineCompletion(
    QNetworkReply *reply,
    const QJsonObject &request,
    const QString &accumulatedResponse,
    const LLMConfig &config)
{
    QString cleanedResponse = accumulatedResponse;

    int newlinePos = cleanedResponse.indexOf('\n');
    if (newlinePos != -1) {
        QString singleLineCompletion = cleanedResponse.left(newlinePos).trimmed();
        singleLineCompletion
            = removeStopWords(singleLineCompletion, config.promptTemplate->stopWords());
        emit completionReceived(singleLineCompletion, request, true);
        m_accumulatedResponses.remove(reply);
        reply->abort();
        return true;
    }
    return false;
}

QString RequestHandler::removeStopWords(const QStringView &completion, const QStringList &stopWords)
{
    QString filteredCompletion = completion.toString();

    for (const QString &stopWord : stopWords) {
        filteredCompletion = filteredCompletion.replace(stopWord, "");
    }

    return filteredCompletion;
}

} // namespace QodeAssist::LLMCore
