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

#include "OpenAICompatProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "PromptTemplateManager.hpp"
#include "QodeAssistSettings.hpp"
#include "settings/PresetPromptsSettings.hpp"

namespace QodeAssist::Providers {

OpenAICompatProvider::OpenAICompatProvider() {}

QString OpenAICompatProvider::name() const
{
    return "OpenAI Compatible (experimental)";
}

QString OpenAICompatProvider::url() const
{
    return "http://localhost:1234";
}

QString OpenAICompatProvider::completionEndpoint() const
{
    return "/v1/chat/completions";
}

void OpenAICompatProvider::prepareRequest(QJsonObject &request)
{
    auto &settings = Settings::presetPromptsSettings();
    const auto &currentTemplate = PromptTemplateManager::instance().getCurrentTemplate();
    if (currentTemplate->name() == "Custom Template")
        return;

    if (request.contains("prompt")) {
        QJsonArray messages{
            {QJsonObject{{"role", "user"}, {"content", request.take("prompt").toString()}}}};
        request["messages"] = std::move(messages);
    }

    request["max_tokens"] = settings.maxTokens();
    request["temperature"] = settings.temperature();
    request["stop"] = QJsonArray::fromStringList(currentTemplate->stopWords());
    if (settings.useTopP())
        request["top_p"] = settings.topP();
    if (settings.useTopK())
        request["top_k"] = settings.topK();
    if (settings.useFrequencyPenalty())
        request["frequency_penalty"] = settings.frequencyPenalty();
    if (settings.usePresencePenalty())
        request["presence_penalty"] = settings.presencePenalty();

    const QString &apiKey = settings.apiKey.value();
    if (!apiKey.isEmpty()) {
        request["api_key"] = apiKey;
    }
}

bool OpenAICompatProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
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

QList<QString> OpenAICompatProvider::getInstalledModels(const Utils::Environment &env)
{
    return QStringList();
}

} // namespace QodeAssist::Providers
