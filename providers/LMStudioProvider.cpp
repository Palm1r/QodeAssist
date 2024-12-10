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

#include "LMStudioProvider.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "llmcore/OpenAIMessage.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"

namespace QodeAssist::Providers {

LMStudioProvider::LMStudioProvider() {}

QString LMStudioProvider::name() const
{
    return "LM Studio";
}

QString LMStudioProvider::url() const
{
    return "http://localhost:1234";
}

QString LMStudioProvider::completionEndpoint() const
{
    return "/v1/chat/completions";
}

QString LMStudioProvider::chatEndpoint() const
{
    return "/v1/chat/completions";
}

bool LMStudioProvider::supportsModelListing() const
{
    return true;
}

void LMStudioProvider::prepareRequest(QJsonObject &request, LLMCore::RequestType type)
{
    auto prepareMessages = [](QJsonObject &req) -> QJsonArray {
        QJsonArray messages;
        if (req.contains("system")) {
            messages.append(
                QJsonObject{{"role", "system"}, {"content", req.take("system").toString()}});
        }
        if (req.contains("prompt")) {
            messages.append(
                QJsonObject{{"role", "user"}, {"content", req.take("prompt").toString()}});
        }
        return messages;
    };

    auto applyModelParams = [&request](const auto &settings) {
        request["max_tokens"] = settings.maxTokens();
        request["temperature"] = settings.temperature();

        if (settings.useTopP())
            request["top_p"] = settings.topP();
        if (settings.useTopK())
            request["top_k"] = settings.topK();
        if (settings.useFrequencyPenalty())
            request["frequency_penalty"] = settings.frequencyPenalty();
        if (settings.usePresencePenalty())
            request["presence_penalty"] = settings.presencePenalty();
    };

    QJsonArray messages = prepareMessages(request);
    if (!messages.isEmpty()) {
        request["messages"] = std::move(messages);
    }

    if (type == LLMCore::RequestType::Fim) {
        applyModelParams(Settings::codeCompletionSettings());
    } else {
        applyModelParams(Settings::chatAssistantSettings());
    }
}

bool LMStudioProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
{
    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        return false;
    }

    auto message = LLMCore::OpenAIMessage::fromJson(data);
    if (message.hasError()) {
        LOG_MESSAGE("Error in OpenAI response: " + message.error);
        return false;
    }

    accumulatedResponse += message.getContent();
    return message.isDone();
}

QList<QString> LMStudioProvider::getInstalledModels(const QString &url)
{
    QList<QString> models;
    QNetworkAccessManager manager;
    QNetworkRequest request(QString("%1%2").arg(url, "/v1/models"));

    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();
        QJsonArray modelArray = jsonObject["data"].toArray();

        for (const QJsonValue &value : modelArray) {
            QJsonObject modelObject = value.toObject();
            QString modelId = modelObject["id"].toString();
            models.append(modelId);
        }
    } else {
        LOG_MESSAGE(QString("Error fetching models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

} // namespace QodeAssist::Providers
