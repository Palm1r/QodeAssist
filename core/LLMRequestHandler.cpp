/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include "LLMRequestHandler.hpp"
#include "LLMProvidersManager.hpp"
#include "QodeAssistUtils.hpp"
#include "settings/GeneralSettings.hpp"

#include <QJsonDocument>
#include <QNetworkReply>

namespace QodeAssist {

LLMRequestHandler::LLMRequestHandler(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{}

void LLMRequestHandler::sendLLMRequest(const LLMConfig &config, const QJsonObject &request)
{
    logMessage(QString("Sending request to llm: \nurl: %1\nRequest body:\n%2")
                   .arg(config.url.toString(),
                        QString::fromUtf8(
                            QJsonDocument(config.providerRequest).toJson(QJsonDocument::Indented))));

    QNetworkRequest networkRequest(config.url);
    prepareNetworkRequest(networkRequest, config.providerRequest);

    QNetworkReply *reply = m_manager->post(networkRequest,
                                           QJsonDocument(config.providerRequest).toJson());
    if (!reply) {
        logMessage("Error: Failed to create network reply");
        return;
    }

    QString requestId = request["id"].toString();
    m_activeRequests[requestId] = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, request, config]() {
        handleLLMResponse(reply, request, config);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        reply->deleteLater();
        m_activeRequests.remove(requestId);
        if (reply->error() != QNetworkReply::NoError) {
            logMessage(QString("Error in QodeAssist request: %1").arg(reply->errorString()));
            emit requestFinished(requestId, false, reply->errorString());
        } else {
            logMessage("Request finished successfully");
            emit requestFinished(requestId, true, QString());
        }
    });
}

void LLMRequestHandler::handleLLMResponse(QNetworkReply *reply,
                                          const QJsonObject &request,
                                          const LLMConfig &config)
{
    QString &accumulatedResponse = m_accumulatedResponses[reply];

    bool isComplete = config.provider->handleResponse(reply, accumulatedResponse);

    if (config.requestType == RequestType::Fim) {
        if (!Settings::generalSettings().multiLineCompletion()
            && processSingleLineCompletion(reply, request, accumulatedResponse, config)) {
            return;
        }
    }

    if (isComplete || reply->isFinished()) {
        if (isComplete) {
            if (config.requestType == RequestType::Fim) {
                auto cleanedCompletion = removeStopWords(accumulatedResponse,
                                                         config.promptTemplate->stopWords());
                emit completionReceived(cleanedCompletion, request, true);
            } else {
                emit completionReceived(accumulatedResponse, request, true);
            }
        } else {
            emit completionReceived(accumulatedResponse, request, false);
        }
        m_accumulatedResponses.remove(reply);
    }
}

bool LLMRequestHandler::cancelRequest(const QString &id)
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

void LLMRequestHandler::prepareNetworkRequest(QNetworkRequest &networkRequest,
                                              const QJsonObject &providerRequest)
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (providerRequest.contains("api_key")) {
        QString apiKey = providerRequest["api_key"].toString();
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    }
}

bool LLMRequestHandler::processSingleLineCompletion(QNetworkReply *reply,
                                                    const QJsonObject &request,
                                                    const QString &accumulatedResponse,
                                                    const LLMConfig &config)
{
    int newlinePos = accumulatedResponse.indexOf('\n');

    if (newlinePos != -1) {
        QString singleLineCompletion = accumulatedResponse.left(newlinePos).trimmed();
        singleLineCompletion = removeStopWords(singleLineCompletion,
                                               config.promptTemplate->stopWords());

        emit completionReceived(singleLineCompletion, request, true);
        m_accumulatedResponses.remove(reply);
        reply->abort();

        return true;
    }
    return false;
}

QString LLMRequestHandler::removeStopWords(const QStringView &completion,
                                           const QStringList &stopWords)
{
    QString filteredCompletion = completion.toString();

    for (const QString &stopWord : stopWords) {
        filteredCompletion = filteredCompletion.replace(stopWord, "");
    }

    return filteredCompletion;
}

} // namespace QodeAssist
