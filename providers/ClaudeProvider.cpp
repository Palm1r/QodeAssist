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

#include "ClaudeProvider.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

ClaudeProvider::ClaudeProvider() {}

QString ClaudeProvider::name() const
{
    return "Claude";
}

QString ClaudeProvider::url() const
{
    return "https://api.anthropic.com";
}

QString ClaudeProvider::completionEndpoint() const
{
    return "/v1/messages";
}

QString ClaudeProvider::chatEndpoint() const
{
    return "/v1/messages";
}

bool ClaudeProvider::supportsModelListing() const
{
    return true;
}

void ClaudeProvider::prepareRequest(QJsonObject &request, LLMCore::RequestType type)
{
    auto applySettings = [&request](const auto &settings) {
        request["max_tokens"] = settings.maxTokens();
        request["temperature"] = settings.temperature();

        if (settings.useTopP())
            request["top_p"] = settings.topP();
        if (settings.useFrequencyPenalty())
            request["frequency_penalty"] = settings.frequencyPenalty();

        request["anthropic-version"] = "2023-06-01";
        request["stream"] = true;
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applySettings(Settings::codeCompletionSettings());
    } else {
        applySettings(Settings::chatAssistantSettings());
    }
}

bool ClaudeProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
{
    bool isComplete = false;
    QString tempResponse = accumulatedResponse;

    while (reply->canReadLine()) {
        QByteArray line = reply->readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        if (!line.startsWith("data:")) {
            continue;
        }

        line = line.mid(6);

        if (line == "[DONE]") {
            isComplete = true;
            break;
        }

        QJsonDocument jsonResponse = QJsonDocument::fromJson(line);
        if (jsonResponse.isNull()) {
            LOG_MESSAGE("Invalid JSON response from Claude: " + QString::fromUtf8(line));
            continue;
        }

        QJsonObject responseObj = jsonResponse.object();

        if (responseObj.contains("error")) {
            LOG_MESSAGE(
                "Claude error: "
                + QString::fromUtf8(QJsonDocument(responseObj).toJson(QJsonDocument::Indented)));
            return false;
        }

        if (responseObj.contains("type") && responseObj["type"].toString() == "message_delta") {
            if (responseObj.contains("delta")) {
                QJsonObject delta = responseObj["delta"].toObject();
                if (delta.contains("text")) {
                    QString text = delta["text"].toString();
                    if (!text.isEmpty()) {
                        tempResponse += text;
                    }
                }
            }

            if (responseObj.contains("usage")) {
                QJsonObject usage = responseObj["usage"].toObject();
                LOG_MESSAGE(QString("Token usage - Input: %1, Output: %2")
                                .arg(usage["input_tokens"].toInt())
                                .arg(usage["output_tokens"].toInt()));
            }
        }

        if (responseObj.contains("message")
            && responseObj["message"].toObject()["status"].toString() == "complete") {
            isComplete = true;
        }
    }

    if (!tempResponse.isEmpty()) {
        accumulatedResponse = tempResponse;
    }

    return isComplete;
}

QList<QString> ClaudeProvider::getInstalledModels(const QString &baseUrl)
{
    QList<QString> models;
    QNetworkAccessManager manager;

    QUrl url(baseUrl + "/v1/models");
    QUrlQuery query;
    query.addQueryItem("limit", "1000");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("anthropic-version", "2023-06-01");

    if (!apiKey().isEmpty()) {
        request.setRawHeader("x-api-key", apiKey().toUtf8());
    }

    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();

        if (jsonObject.contains("data")) {
            QJsonArray modelArray = jsonObject["data"].toArray();
            for (const QJsonValue &value : modelArray) {
                QJsonObject modelObject = value.toObject();
                if (modelObject.contains("id")) {
                    QString modelId = modelObject["id"].toString();
                    models.append(modelId);
                }
            }
        }
    } else {
        LOG_MESSAGE(QString("Error fetching Claude models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> ClaudeProvider::validateRequest(const QJsonObject &request, LLMCore::TemplateType type)
{}

QString ClaudeProvider::apiKey() const
{
    return Settings::providerSettings().claudeApiKey();
}

} // namespace QodeAssist::Providers
