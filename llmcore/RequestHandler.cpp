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

namespace QodeAssist::LLMCore {

RequestHandler::RequestHandler(QObject *parent)
    : RequestHandlerBase(parent)
{}

void RequestHandler::sendLLMRequest(const LLMConfig &config, const QJsonObject &request)
{
    LOG_MESSAGE(QString("Sending request to llm: \nurl: %1\nRequest body:\n%2")
                    .arg(
                        config.url.toString(),
                        QString::fromUtf8(
                            QJsonDocument(config.providerRequest).toJson(QJsonDocument::Indented))));

    QNetworkAccessManager *manager = new QNetworkAccessManager();
    QNetworkRequest networkRequest(config.url);
    config.provider->prepareNetworkRequest(networkRequest);

    QNetworkReply *reply
        = manager->post(networkRequest, QJsonDocument(config.providerRequest).toJson());
    if (!reply) {
        LOG_MESSAGE("Error: Failed to create network reply");
        return;
    }

    QString requestId = request["id"].toString();
    m_activeRequests[requestId] = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, request, config]() {
        handleLLMResponse(reply, request, config);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId, manager]() {
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
        manager->deleteLater();
    });
}

void RequestHandler::handleLLMResponse(
    QNetworkReply *reply, const QJsonObject &request, const LLMConfig &config)
{
    if (!reply) {
        LOG_MESSAGE("Error: Null reply in handleLLMResponse");
        return;
    }

    if (!m_accumulatedResponses.contains(reply)) {
        m_accumulatedResponses[reply] = QString();
    }

    QString &accumulatedResponse = m_accumulatedResponses[reply];

    bool isComplete = false;

    try {
        if (config.provider) {
            isComplete = config.provider->handleResponse(reply, accumulatedResponse);
        } else {
            LOG_MESSAGE("Error: Provider is null in handleLLMResponse");
            m_accumulatedResponses.remove(reply);
            return;
        }
    } catch (const std::exception &e) {
        LOG_MESSAGE(QString("Exception in handleResponse: %1").arg(e.what()));
        m_accumulatedResponses.remove(reply);
        return;
    } catch (...) {
        LOG_MESSAGE("Unknown exception in handleResponse");
        m_accumulatedResponses.remove(reply);
        return;
    }

    if (config.requestType == RequestType::CodeCompletion) {
        if (!config.multiLineCompletion
            && processSingleLineCompletion(reply, request, accumulatedResponse, config)) {
            return;
        }

        if (isComplete) {
            try {
                QStringList stopWords;
                if (config.promptTemplate) {
                    stopWords = config.promptTemplate->stopWords();
                }
                auto cleanedCompletion = removeStopWords(accumulatedResponse, stopWords);
                emit completionReceived(cleanedCompletion, request, true);
            } catch (const std::exception &e) {
                LOG_MESSAGE(QString("Exception in completion processing: %1").arg(e.what()));
            } catch (...) {
                LOG_MESSAGE("Unknown exception in completion processing");
            }
        }
    } else if (config.requestType == RequestType::Chat) {
        try {
            emit completionReceived(accumulatedResponse, request, isComplete);
        } catch (const std::exception &e) {
            LOG_MESSAGE(QString("Exception in chat response processing: %1").arg(e.what()));
        } catch (...) {
            LOG_MESSAGE("Unknown exception in chat response processing");
        }
    }

    if (isComplete) {
        m_accumulatedResponses.remove(reply);
    }
}

bool RequestHandler::cancelRequest(const QString &id)
{
    if (m_activeRequests.contains(id)) {
        QNetworkReply *reply = m_activeRequests[id];
        reply->abort();
        m_activeRequests.remove(id);
        m_accumulatedResponses.remove(reply);
        emit requestCancelled(id);
        return true;
    }
    return false;
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
