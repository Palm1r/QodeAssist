/*
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include "OpenAIProvider.hpp"

#include <llmcore/clients/openai/OpenAIClient.hpp>
#include "tools/ToolsFactory.hpp"

#include <llmcore/prompt/ValidationUtils.hpp>
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

#include <QJsonArray>
#include <QJsonObject>

namespace QodeAssist::Providers {

OpenAIProvider::OpenAIProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_client(new LLMCore::OpenAIClient([this]() { return apiKey(); }, this))
{
    Tools::registerAppTools(m_client->tools());
    Tools::configureToolSettings(m_client->tools());
    connectClientSignals(m_client);
}

QString OpenAIProvider::name() const
{
    return "OpenAI";
}

QString OpenAIProvider::url() const
{
    return "https://api.openai.com";
}

QString OpenAIProvider::completionEndpoint() const
{
    return "/v1/chat/completions";
}

QString OpenAIProvider::chatEndpoint() const
{
    return "/v1/chat/completions";
}

bool OpenAIProvider::supportsModelListing() const
{
    return true;
}

void OpenAIProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
    LLMCore::ContextData context,
    LLMCore::RequestType type,
    bool isToolsEnabled,
    bool isThinkingEnabled)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
    }

    prompt->prepareRequest(request, context);

    auto applyModelParams = [&request](const auto &settings) {
        QString model = request.value("model").toString().toLower();
        bool useNewParameter = model.contains("gpt-4o") || model.contains("gpt-4-turbo")
                               || model.contains("o1-") || model.contains("gpt-5")
                               || model.startsWith("o1") || model.contains("o3");

        bool isReasoningModel = model.contains("o1-") || model.contains("gpt-5")
                                || model.startsWith("o1") || model.contains("o3");

        if (useNewParameter) {
            request["max_completion_tokens"] = settings.maxTokens();
        } else {
            request["max_tokens"] = settings.maxTokens();
        }

        if (!isReasoningModel) {
            request["temperature"] = settings.temperature();

            if (settings.useTopP())
                request["top_p"] = settings.topP();
            if (settings.useTopK())
                request["top_k"] = settings.topK();

        } else {
            request["temperature"] = 1.0;
        }

        if (settings.useFrequencyPenalty())
            request["frequency_penalty"] = settings.frequencyPenalty();
        if (settings.usePresencePenalty())
            request["presence_penalty"] = settings.presencePenalty();
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == LLMCore::RequestType::QuickRefactoring) {
        applyModelParams(Settings::quickRefactorSettings());
    } else {
        applyModelParams(Settings::chatAssistantSettings());
    }

    if (isToolsEnabled) {
        LLMCore::RunToolsFilter filter = LLMCore::RunToolsFilter::ALL;
        if (type == LLMCore::RequestType::QuickRefactoring) {
            filter = LLMCore::RunToolsFilter::OnlyRead;
        }

        auto toolsDefinitions = m_client->tools()->getToolsDefinitions(
            LLMCore::ToolSchemaFormat::OpenAI, filter);
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to OpenAI request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> OpenAIProvider::getInstalledModels(const QString &url)
{

    return m_client->listModels(url).then([](const QList<QString> &models) {
        QList<QString> filtered;
        for (const auto &modelId : models) {
            if (!modelId.contains("dall-e") && !modelId.contains("whisper")
                && !modelId.contains("tts") && !modelId.contains("davinci")
                && !modelId.contains("babbage") && !modelId.contains("omni")) {
                filtered.append(modelId);
            }
        }
        return filtered;
    });
}

QList<QString> OpenAIProvider::validateRequest(const QJsonObject &request, LLMCore::TemplateType type)
{
    const auto templateReq = QJsonObject{
        {"model", {}},
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
        {"temperature", {}},
        {"max_tokens", {}},
        {"max_completion_tokens", {}},
        {"top_p", {}},
        {"top_k", {}},
        {"frequency_penalty", {}},
        {"presence_penalty", {}},
        {"stop", QJsonArray{}},
        {"stream", {}},
        {"tools", {}}};

    return LLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString OpenAIProvider::apiKey() const
{
    return Settings::providerSettings().openAiApiKey();
}

LLMCore::ProviderID OpenAIProvider::providerID() const
{
    return LLMCore::ProviderID::OpenAI;
}

void OpenAIProvider::sendRequest(
    const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{

    m_client->sendMessage(requestId, url, payload);
}

LLMCore::ProviderCapabilities OpenAIProvider::capabilities() const
{
    return LLMCore::ToolsCapability | LLMCore::ImageCapability;
}

void OpenAIProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    m_client->cancelRequest(requestId);
}

LLMCore::ToolsManager *OpenAIProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
