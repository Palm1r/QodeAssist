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
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "logger/Logger.hpp"

namespace QodeAssist::Providers {

OpenAICompatProvider::OpenAICompatProvider() {}

QString OpenAICompatProvider::name() const
{
    return "OpenAI Compatible";
}

QString OpenAICompatProvider::url() const
{
    return "http://localhost:1234";
}

QString OpenAICompatProvider::completionEndpoint() const
{
    return "/v1/chat/completions";
}

QString OpenAICompatProvider::chatEndpoint() const
{
    return "/v1/chat/completions";
}

bool OpenAICompatProvider::supportsModelListing() const
{
    return false;
}

void OpenAICompatProvider::prepareRequest(QJsonObject &request, LLMCore::RequestType type)
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

bool OpenAICompatProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
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
            LOG_MESSAGE(
                "Invalid JSON response from OpenAI compatible provider: " + QString::fromUtf8(line));
            continue;
        }

        QJsonObject responseObj = jsonResponse.object();

        if (responseObj.contains("error")) {
            LOG_MESSAGE(
                "OpenAI compatible provider error: "
                + QString::fromUtf8(QJsonDocument(responseObj).toJson(QJsonDocument::Indented)));
            return false;
        }

        if (responseObj.contains("choices")) {
            QJsonArray choices = responseObj["choices"].toArray();
            if (!choices.isEmpty()) {
                QJsonObject choice = choices.first().toObject();
                QJsonObject delta = choice["delta"].toObject();
                if (delta.contains("content")) {
                    QString completion = delta["content"].toString();
                    if (!completion.isEmpty()) {
                        tempResponse += completion;
                    }
                }
                QString finishReason = choice["finish_reason"].toString();
                if (!finishReason.isNull() && finishReason == "stop") {
                    isComplete = true;
                }
            }
        }

        if (responseObj.contains("usage")) {
            QJsonObject usage = responseObj["usage"].toObject();
            LOG_MESSAGE(QString("Token usage - Prompt: %1, Completion: %2, Total: %3")
                            .arg(usage["prompt_tokens"].toInt())
                            .arg(usage["completion_tokens"].toInt())
                            .arg(usage["total_tokens"].toInt()));
        }
    }

    if (!tempResponse.isEmpty()) {
        accumulatedResponse = tempResponse;
    }

    return isComplete;
}

QList<QString> OpenAICompatProvider::getInstalledModels(const QString &url)
{
    return QStringList();
}

} // namespace QodeAssist::Providers
