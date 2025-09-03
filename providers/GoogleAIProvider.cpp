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

#include "GoogleAIProvider.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QtCore/qurlquery.h>

#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

QString GoogleAIProvider::name() const
{
    return "Google AI";
}

QString GoogleAIProvider::url() const
{
    return "https://generativelanguage.googleapis.com/v1beta";
}

QString GoogleAIProvider::completionEndpoint() const
{
    return {};
}

QString GoogleAIProvider::chatEndpoint() const
{
    return {};
}

bool GoogleAIProvider::supportsModelListing() const
{
    return true;
}

void GoogleAIProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
    LLMCore::ContextData context,
    LLMCore::RequestType type)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
    }

    prompt->prepareRequest(request, context);

    auto applyModelParams = [&request](const auto &settings) {
        QJsonObject generationConfig;
        generationConfig["maxOutputTokens"] = settings.maxTokens();
        generationConfig["temperature"] = settings.temperature();

        if (settings.useTopP())
            generationConfig["topP"] = settings.topP();
        if (settings.useTopK())
            generationConfig["topK"] = settings.topK();

        request["generationConfig"] = generationConfig;
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else {
        applyModelParams(Settings::chatAssistantSettings());
    }
}

QList<QString> GoogleAIProvider::getInstalledModels(const QString &url)
{
    QList<QString> models;

    QNetworkAccessManager manager;
    QNetworkRequest request(QString("%1/models?key=%2").arg(url, apiKey()));

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();

        if (jsonObject.contains("models")) {
            QJsonArray modelArray = jsonObject["models"].toArray();
            models.clear();

            for (const QJsonValue &value : modelArray) {
                QJsonObject modelObject = value.toObject();
                if (modelObject.contains("name")) {
                    QString modelName = modelObject["name"].toString();
                    if (modelName.contains("/")) {
                        modelName = modelName.split("/").last();
                    }
                    models.append(modelName);
                }
            }
        }
    } else {
        LOG_MESSAGE(QString("Error fetching Google AI models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> GoogleAIProvider::validateRequest(
    const QJsonObject &request, LLMCore::TemplateType type)
{
    QJsonObject templateReq;

    templateReq = QJsonObject{
        {"contents", QJsonArray{}},
        {"system_instruction", QJsonArray{}},
        {"generationConfig",
         QJsonObject{{"temperature", {}}, {"maxOutputTokens", {}}, {"topP", {}}, {"topK", {}}}},
        {"safetySettings", QJsonArray{}}};

    return LLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString GoogleAIProvider::apiKey() const
{
    return Settings::providerSettings().googleAiApiKey();
}

void GoogleAIProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QUrl url = networkRequest.url();
    QUrlQuery query(url.query());
    query.addQueryItem("key", apiKey());
    url.setQuery(query);
    networkRequest.setUrl(url);
}

LLMCore::ProviderID GoogleAIProvider::providerID() const
{
    return LLMCore::ProviderID::GoogleAI;
}

void GoogleAIProvider::sendRequest(
    const QString &requestId, const QUrl &url, const QJsonObject &payload)
{
    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = payload};

    LOG_MESSAGE(
        QString("GoogleAIProvider: Sending request %1 to %2").arg(requestId, url.toString()));

    emit httpClient()->sendRequest(request);
}

void GoogleAIProvider::onDataReceived(const QString &requestId, const QByteArray &data)
{
    QString &accumulatedResponse = m_accumulatedResponses[requestId];

    if (data.isEmpty()) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("error")) {
            QJsonObject error = obj["error"].toObject();
            QString errorMessage = error["message"].toString();
            int errorCode = error["code"].toInt();
            QString fullError
                = QString("Google AI API Error %1: %2").arg(errorCode).arg(errorMessage);

            LOG_MESSAGE(fullError);
            emit requestFailed(requestId, fullError);
            m_accumulatedResponses.remove(requestId);
            return;
        }
    }

    bool isDone = false;

    if (data.startsWith("data: ")) {
        isDone = handleStreamResponse(requestId, data, accumulatedResponse);
    } else {
        isDone = handleRegularResponse(requestId, data, accumulatedResponse);
    }

    if (isDone) {
        emit fullResponseReceived(requestId, accumulatedResponse);
        m_accumulatedResponses.remove(requestId);
    }
}

void GoogleAIProvider::onRequestFinished(const QString &requestId, bool success, const QString &error)
{
    if (!success) {
        QString detailedError = error;

        if (m_accumulatedResponses.contains(requestId)) {
            const QString response = m_accumulatedResponses[requestId];
            if (!response.isEmpty()) {
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &parseError);
                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject obj = doc.object();
                    if (obj.contains("error")) {
                        QJsonObject errorObj = obj["error"].toObject();
                        QString apiError = errorObj["message"].toString();
                        int errorCode = errorObj["code"].toInt();
                        detailedError
                            = QString("Google AI API Error %1: %2").arg(errorCode).arg(apiError);
                    }
                }
            }
        }

        LOG_MESSAGE(QString("GoogleAIProvider request %1 failed: %2").arg(requestId, detailedError));
        emit requestFailed(requestId, detailedError);
    } else {
        if (m_accumulatedResponses.contains(requestId)) {
            const QString fullResponse = m_accumulatedResponses[requestId];
            if (!fullResponse.isEmpty()) {
                emit fullResponseReceived(requestId, fullResponse);
            }
        }
    }

    m_accumulatedResponses.remove(requestId);
}

bool GoogleAIProvider::handleStreamResponse(
    const QString &requestId, const QByteArray &data, QString &accumulatedResponse)
{
    QByteArrayList lines = data.split('\n');
    bool isDone = false;

    for (const QByteArray &line : lines) {
        QByteArray trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }

        if (trimmedLine == "data: [DONE]") {
            isDone = true;
            continue;
        }

        if (trimmedLine.startsWith("data: ")) {
            QByteArray jsonData = trimmedLine.mid(6);
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
            if (doc.isNull() || !doc.isObject()) {
                if (parseError.error != QJsonParseError::NoError) {
                    LOG_MESSAGE(QString("JSON parse error in GoogleAI stream: %1")
                                    .arg(parseError.errorString()));
                }
                continue;
            }

            QJsonObject responseObj = doc.object();

            if (responseObj.contains("error")) {
                QJsonObject error = responseObj["error"].toObject();
                QString errorMessage = error["message"].toString();
                int errorCode = error["code"].toInt();
                QString fullError
                    = QString("Google AI Stream Error %1: %2").arg(errorCode).arg(errorMessage);

                LOG_MESSAGE(fullError);
                emit requestFailed(requestId, fullError);
                return true;
            }

            if (responseObj.contains("candidates")) {
                QJsonArray candidates = responseObj["candidates"].toArray();
                if (!candidates.isEmpty()) {
                    QJsonObject candidate = candidates.first().toObject();

                    if (candidate.contains("finishReason")
                        && !candidate["finishReason"].toString().isEmpty()) {
                        isDone = true;
                    }

                    if (candidate.contains("content")) {
                        QJsonObject content = candidate["content"].toObject();
                        if (content.contains("parts")) {
                            QJsonArray parts = content["parts"].toArray();
                            QString partialContent;
                            for (const auto &part : parts) {
                                QJsonObject partObj = part.toObject();
                                if (partObj.contains("text")) {
                                    partialContent += partObj["text"].toString();
                                }
                            }
                            if (!partialContent.isEmpty()) {
                                accumulatedResponse += partialContent;
                                emit partialResponseReceived(requestId, partialContent);
                            }
                        }
                    }
                }
            }
        }
    }

    return isDone;
}

bool GoogleAIProvider::handleRegularResponse(
    const QString &requestId, const QByteArray &data, QString &accumulatedResponse)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        QString error
            = QString("Invalid JSON response from Google AI API: %1").arg(parseError.errorString());
        LOG_MESSAGE(error);
        emit requestFailed(requestId, error);
        return false;
    }

    QJsonObject response = doc.object();

    if (response.contains("error")) {
        QJsonObject error = response["error"].toObject();
        QString errorMessage = error["message"].toString();
        int errorCode = error["code"].toInt();
        QString fullError = QString("Google AI API Error %1: %2").arg(errorCode).arg(errorMessage);

        LOG_MESSAGE(fullError);
        emit requestFailed(requestId, fullError);
        return false;
    }

    if (!response.contains("candidates") || response["candidates"].toArray().isEmpty()) {
        QString error = "No candidates in Google AI response";
        LOG_MESSAGE(error);
        emit requestFailed(requestId, error);
        return false;
    }

    QJsonObject candidate = response["candidates"].toArray().first().toObject();
    if (!candidate.contains("content")) {
        QString error = "No content in Google AI response candidate";
        LOG_MESSAGE(error);
        emit requestFailed(requestId, error);
        return false;
    }

    QJsonObject content = candidate["content"].toObject();
    if (!content.contains("parts")) {
        QString error = "No parts in Google AI response content";
        LOG_MESSAGE(error);
        emit requestFailed(requestId, error);
        return false;
    }

    QJsonArray parts = content["parts"].toArray();
    QString responseContent;
    for (const auto &part : parts) {
        QJsonObject partObj = part.toObject();
        if (partObj.contains("text")) {
            responseContent += partObj["text"].toString();
        }
    }

    if (!responseContent.isEmpty()) {
        accumulatedResponse += responseContent;
        emit partialResponseReceived(requestId, responseContent);
    }

    return true;
}

} // namespace QodeAssist::Providers
