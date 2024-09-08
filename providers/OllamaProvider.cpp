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

void OllamaProvider::prepareRequest(QJsonObject &request)
{
    auto &settings = Settings::presetPromptsSettings();
    auto currentTemplate = PromptTemplateManager::instance().getCurrentTemplate();
    if (currentTemplate->name() == "Custom Template")
        return;

    QJsonObject options;
    options["num_predict"] = settings.maxTokens();
    options["keep_alive"] = settings.ollamaLivetime();
    options["temperature"] = settings.temperature();
    options["stop"] = QJsonArray::fromStringList(currentTemplate->stopWords());
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
    bool isComplete = false;
    while (reply->canReadLine()) {
        QByteArray line = reply->readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QJsonDocument jsonResponse = QJsonDocument::fromJson(line);
        if (jsonResponse.isNull()) {
            qWarning() << "Invalid JSON response from Ollama:" << line;
            continue;
        }
        QJsonObject responseObj = jsonResponse.object();
        if (responseObj.contains("response")) {
            QString completion = responseObj["response"].toString();

            accumulatedResponse += completion;
        }
        if (responseObj["done"].toBool()) {
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
