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

bool GoogleAIProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
{
    if (reply->isFinished()) {
        if (reply->bytesAvailable() > 0) {
            QByteArray data = reply->readAll();

            if (data.startsWith("data: ")) {
                return handleStreamResponse(data, accumulatedResponse);
            } else {
                return handleRegularResponse(data, accumulatedResponse);
            }
        }

        return true;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        return false;
    }

    if (data.startsWith("data: ")) {
        return handleStreamResponse(data, accumulatedResponse);
    } else {
        return handleRegularResponse(data, accumulatedResponse);
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

bool GoogleAIProvider::handleStreamResponse(const QByteArray &data, QString &accumulatedResponse)
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
            QByteArray jsonData = trimmedLine.mid(6); // Remove "data: " prefix
            QJsonDocument doc = QJsonDocument::fromJson(jsonData);
            if (doc.isNull() || !doc.isObject()) {
                continue;
            }

            QJsonObject responseObj = doc.object();

            if (responseObj.contains("error")) {
                QJsonObject error = responseObj["error"].toObject();
                LOG_MESSAGE("Error in Google AI stream response: " + error["message"].toString());
                continue;
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
                            for (const auto &part : parts) {
                                QJsonObject partObj = part.toObject();
                                if (partObj.contains("text")) {
                                    accumulatedResponse += partObj["text"].toString();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return isDone;
}

bool GoogleAIProvider::handleRegularResponse(const QByteArray &data, QString &accumulatedResponse)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        LOG_MESSAGE("Invalid JSON response from Google AI API");
        return false;
    }

    QJsonObject response = doc.object();

    if (response.contains("error")) {
        QJsonObject error = response["error"].toObject();
        LOG_MESSAGE("Error in Google AI response: " + error["message"].toString());
        return false;
    }

    if (!response.contains("candidates") || response["candidates"].toArray().isEmpty()) {
        return false;
    }

    QJsonObject candidate = response["candidates"].toArray().first().toObject();
    if (!candidate.contains("content")) {
        return false;
    }

    QJsonObject content = candidate["content"].toObject();
    if (!content.contains("parts")) {
        return false;
    }

    QJsonArray parts = content["parts"].toArray();
    for (const auto &part : parts) {
        QJsonObject partObj = part.toObject();
        if (partObj.contains("text")) {
            accumulatedResponse += partObj["text"].toString();
        }
    }

    return true;
}

} // namespace QodeAssist::Providers
