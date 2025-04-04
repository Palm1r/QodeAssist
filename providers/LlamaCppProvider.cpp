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

#include "LlamaCppProvider.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "llmcore/OpenAIMessage.hpp"
#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"

namespace QodeAssist::Providers {

QString LlamaCppProvider::name() const
{
    return "llama.cpp";
}

QString LlamaCppProvider::url() const
{
    return "http://localhost:8080";
}

QString LlamaCppProvider::completionEndpoint() const
{
    return "/infill";
}

QString LlamaCppProvider::chatEndpoint() const
{
    return "/v1/chat/completions";
}

bool LlamaCppProvider::supportsModelListing() const
{
    return false;
}

void LlamaCppProvider::prepareRequest(
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

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else {
        applyModelParams(Settings::chatAssistantSettings());
    }
}

bool LlamaCppProvider::handleResponse(QNetworkReply *reply, QString &accumulatedResponse)
{
    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        return false;
    }

    bool isDone = data.contains("\"stop\":true") || data.contains("data: [DONE]");

    QByteArrayList lines = data.split('\n');
    for (const QByteArray &line : lines) {
        if (line.trimmed().isEmpty()) {
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

        QJsonObject obj = doc.object();

        if (obj.contains("content")) {
            QString content = obj["content"].toString();
            if (!content.isEmpty()) {
                accumulatedResponse += content;
            }
        } else if (obj.contains("choices")) {
            auto message = LLMCore::OpenAIMessage::fromJson(obj);
            if (message.hasError()) {
                LOG_MESSAGE("Error in llama.cpp response: " + message.error);
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

        if (obj["stop"].toBool()) {
            isDone = true;
        }
    }

    return isDone;
}

QList<QString> LlamaCppProvider::getInstalledModels(const QString &url)
{
    return {};
}

QList<QString> LlamaCppProvider::validateRequest(
    const QJsonObject &request, LLMCore::TemplateType type)
{
    if (type == LLMCore::TemplateType::FIM) {
        const auto infillReq = QJsonObject{
            {"model", {}},
            {"input_prefix", {}},
            {"input_suffix", {}},
            {"input_extra", {}},
            {"prompt", {}},
            {"temperature", {}},
            {"top_p", {}},
            {"top_k", {}},
            {"max_tokens", {}},
            {"frequency_penalty", {}},
            {"presence_penalty", {}},
            {"stop", QJsonArray{}},
            {"stream", {}}};

        return LLMCore::ValidationUtils::validateRequestFields(request, infillReq);
    } else {
        const auto chatReq = QJsonObject{
            {"model", {}},
            {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
            {"temperature", {}},
            {"max_tokens", {}},
            {"top_p", {}},
            {"top_k", {}},
            {"frequency_penalty", {}},
            {"presence_penalty", {}},
            {"stop", QJsonArray{}},
            {"stream", {}}};

        return LLMCore::ValidationUtils::validateRequestFields(request, chatReq);
    }
}

QString LlamaCppProvider::apiKey() const
{
    return {};
}

void LlamaCppProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
}

LLMCore::ProviderID LlamaCppProvider::providerID() const
{
    return LLMCore::ProviderID::LlamaCpp;
}

} // namespace QodeAssist::Providers
