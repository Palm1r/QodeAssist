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

#include "llmcore/OllamaMessage.hpp"
#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"

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

bool OllamaProvider::supportsModelListing() const
{
    return true;
}

void OllamaProvider::prepareRequest(QJsonObject &request, LLMCore::RequestType type)
{
    auto applySettings = [&request](const auto &settings) {
        QJsonObject options;
        options["num_predict"] = settings.maxTokens();
        options["temperature"] = settings.temperature();
        options["stop"] = request.take("stop");

        if (settings.useTopP())
            options["top_p"] = settings.topP();
        if (settings.useTopK())
            options["top_k"] = settings.topK();
        if (settings.useFrequencyPenalty())
            options["frequency_penalty"] = settings.frequencyPenalty();
        if (settings.usePresencePenalty())
            options["presence_penalty"] = settings.presencePenalty();

        request["options"] = options;
        request["keep_alive"] = settings.ollamaLivetime();
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applySettings(Settings::codeCompletionSettings());
    } else {
        applySettings(Settings::chatAssistantSettings());
    }
}

bool OllamaProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
{
    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        return false;
    }

    const QString endpoint = reply->url().path();
    auto messageType = endpoint == completionEndpoint() ? LLMCore::OllamaMessage::Type::Generate
                                                        : LLMCore::OllamaMessage::Type::Chat;

    auto message = LLMCore::OllamaMessage::fromJson(data, messageType);
    if (message.hasError()) {
        LOG_MESSAGE("Error in Ollama response: " + message.error);
        return false;
    }

    accumulatedResponse += message.getContent();
    return message.done;
}

QList<QString> OllamaProvider::getInstalledModels(const QString &url)
{
    QList<QString> models;
    QNetworkAccessManager manager;
    QNetworkRequest request(QString("%1%2").arg(url, "/api/tags"));
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
        LOG_MESSAGE(QString("Error fetching models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> OllamaProvider::validateRequest(const QJsonObject &request, LLMCore::TemplateType type)
{
    const auto fimReq = QJsonObject{
        {"keep_alive", {}},
        {"model", {}},
        {"stream", {}},
        {"prompt", {}},
        {"suffix", {}},
        {"system", {}},
        {"options",
         QJsonObject{
             {"temperature", {}},
             {"stop", {}},
             {"top_p", {}},
             {"top_k", {}},
             {"num_predict", {}},
             {"frequency_penalty", {}},
             {"presence_penalty", {}}}}};

    const auto messageReq = QJsonObject{
        {"keep_alive", {}},
        {"model", {}},
        {"stream", {}},
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
        {"options",
         QJsonObject{
             {"temperature", {}},
             {"stop", {}},
             {"top_p", {}},
             {"top_k", {}},
             {"num_predict", {}},
             {"frequency_penalty", {}},
             {"presence_penalty", {}}}}};

    return LLMCore::ValidationUtils::validateRequestFields(
        request, type == LLMCore::TemplateType::Fim ? fimReq : messageReq);
}

QString OllamaProvider::apiKey() const
{
    return {};
};

} // namespace QodeAssist::Providers
