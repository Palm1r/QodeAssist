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

#include "OllamaProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QtCore/qeventloop.h>

#include "PromptTemplateManager.hpp"
#include "QodeAssistUtils.hpp"
#include "settings/PresetPromptsSettings.hpp"

namespace QodeAssist::Providers {

OllamaProvider::OllamaProvider() {}

QString OllamaProvider::name() const
{
    return "Ollama";
}

QString OllamaProvider::url() const
{
    return "http://localhost:11434";
}

QString OllamaProvider::completionEndpoint() const
{
    return "/api/generate";
}

QString OllamaProvider::chatEndpoint() const
{
    return "/api/chat";
}

void OllamaProvider::prepareRequest(QJsonObject &request)
{
    auto &settings = Settings::presetPromptsSettings();

    QJsonObject options;
    options["num_predict"] = settings.maxTokens();
    options["keep_alive"] = settings.ollamaLivetime();
    options["temperature"] = settings.temperature();
    if (settings.useTopP())
        options["top_p"] = settings.topP();
    if (settings.useTopK())
        options["top_k"] = settings.topK();
    if (settings.useFrequencyPenalty())
        options["frequency_penalty"] = settings.frequencyPenalty();
    if (settings.usePresencePenalty())
        options["presence_penalty"] = settings.presencePenalty();
    request["options"] = options;
}

bool OllamaProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
{
    QString endpoint = reply->url().path();

    bool isComplete = false;
    while (reply->canReadLine()) {
        QByteArray line = reply->readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isNull()) {
            logMessage("Invalid JSON response from Ollama: " + QString::fromUtf8(line));
            continue;
        }

        QJsonObject responseObj = doc.object();

        if (responseObj.contains("error")) {
            QString errorMessage = responseObj["error"].toString();
            logMessage("Error in Ollama response: " + errorMessage);
            return false;
        }

        if (endpoint == completionEndpoint()) {
            if (responseObj.contains("response")) {
                QString completion = responseObj["response"].toString();
                accumulatedResponse += completion;
            }
        } else if (endpoint == chatEndpoint()) {
            if (responseObj.contains("message")) {
                QJsonObject message = responseObj["message"].toObject();
                if (message.contains("content")) {
                    QString content = message["content"].toString();
                    accumulatedResponse += content;
                }
            }
        } else {
            logMessage("Unknown endpoint: " + endpoint);
        }

        if (responseObj.contains("done") && responseObj["done"].toBool()) {
            isComplete = true;
            break;
        }
    }

    return isComplete;
}

QList<QString> OllamaProvider::getInstalledModels(const Utils::Environment &env)
{
    QList<QString> models;
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(url() + "/api/tags"));
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();
        QJsonArray modelArray = jsonObject["models"].toArray();

        for (const QJsonValue &value : modelArray) {
            QJsonObject modelObject = value.toObject();
            QString modelName = modelObject["name"].toString();
            models.append(modelName);
        }
    } else {
        logMessage(QString("Error fetching models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

} // namespace QodeAssist::Providers
