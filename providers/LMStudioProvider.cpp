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

#include "QodeAssistUtils.hpp"
#include "settings/PresetPromptsSettings.hpp"

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

void LMStudioProvider::prepareRequest(QJsonObject &request)
{
    auto &settings = Settings::presetPromptsSettings();
    QJsonArray messages;

    if (request.contains("system")) {
        QJsonObject systemMessage{{"role", "system"},
                                  {"content", request.take("system").toString()}};
        messages.append(systemMessage);
    }

    if (request.contains("prompt")) {
        QJsonObject userMessage{{"role", "user"}, {"content", request.take("prompt").toString()}};
        messages.append(userMessage);
    }

    if (!messages.isEmpty()) {
        request["messages"] = std::move(messages);
    }

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
}

bool LMStudioProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
{
    bool isComplete = false;
    while (reply->canReadLine()) {
        QByteArray line = reply->readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line == "data: [DONE]") {
            isComplete = true;
            break;
        }
        if (line.startsWith("data: ")) {
            line = line.mid(6); // Remove "data: " prefix
        }
        QJsonDocument jsonResponse = QJsonDocument::fromJson(line);
        if (jsonResponse.isNull()) {
            qWarning() << "Invalid JSON response from LM Studio:" << line;
            continue;
        }
        QJsonObject responseObj = jsonResponse.object();
        if (responseObj.contains("choices")) {
            QJsonArray choices = responseObj["choices"].toArray();
            if (!choices.isEmpty()) {
                QJsonObject choice = choices.first().toObject();
                QJsonObject delta = choice["delta"].toObject();
                if (delta.contains("content")) {
                    QString completion = delta["content"].toString();

                    accumulatedResponse += completion;
                }
                if (choice["finish_reason"].toString() == "stop") {
                    isComplete = true;
                    break;
                }
            }
        }
    }
    return isComplete;
}

QList<QString> LMStudioProvider::getInstalledModels(const Utils::Environment &env,
                                                    const QString &url)
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
        logMessage(QString("Error fetching models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

} // namespace QodeAssist::Providers
