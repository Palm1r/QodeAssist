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

#include "OpenRouterAIProvider.hpp"

#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/ProviderSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "llmcore/OpenAIMessage.hpp"
#include "logger/Logger.hpp"

namespace QodeAssist::Providers {

OpenRouterProvider::OpenRouterProvider() {}

QString OpenRouterProvider::name() const
{
    return "OpenRouter";
}

QString OpenRouterProvider::url() const
{
    return "https://openrouter.ai/api";
}

void OpenRouterProvider::prepareRequest(QJsonObject &request, LLMCore::RequestType type)
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

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else {
        applyModelParams(Settings::chatAssistantSettings());
    }
}

bool OpenRouterProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
{
    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        return false;
    }

    bool isDone = false;
    QByteArrayList lines = data.split('\n');

    for (const QByteArray &line : lines) {
        if (line.trimmed().isEmpty() || line.contains("OPENROUTER PROCESSING")) {
            continue;
        }

        if (line == "data: [DONE]") {
            isDone = true;
            continue;
        }

        QByteArray jsonData = line;
        if (line.startsWith("data: ")) {
            jsonData = line.mid(6);
        }

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);

        if (doc.isNull()) {
            continue;
        }

        auto message = LLMCore::OpenAIMessage::fromJson(doc.object());
        if (message.hasError()) {
            LOG_MESSAGE("Error in OpenAI response: " + message.error);
            continue;
        }

        QString content = message.getContent();
        if (!content.isEmpty()) {
            accumulatedResponse += content;
        }

        if (message.isDone()) {
            isDone = true;
        }
    }

    return isDone;
}

QString OpenRouterProvider::apiKey() const
{
    return Settings::providerSettings().openRouterApiKey();
}

} // namespace QodeAssist::Providers
