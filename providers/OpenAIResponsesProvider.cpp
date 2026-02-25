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

#include "OpenAIResponsesProvider.hpp"

#include "llmcore/OpenAIResponsesClient.hpp"
#include "tools/ToolsFactory.hpp"

#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QodeAssist::Providers {

OpenAIResponsesProvider::OpenAIResponsesProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_client(new LLMCore::OpenAIResponsesClient([this]() { return apiKey(); }, this))
{
    Tools::registerAppTools(m_client->tools());
    Tools::configureToolSettings(m_client->tools());
    connectClientSignals(m_client);
}

QString OpenAIResponsesProvider::name() const
{
    return "OpenAI Responses";
}

QString OpenAIResponsesProvider::url() const
{
    return "https://api.openai.com";
}

QString OpenAIResponsesProvider::completionEndpoint() const
{
    return "/v1/responses";
}

QString OpenAIResponsesProvider::chatEndpoint() const
{
    return "/v1/responses";
}

bool OpenAIResponsesProvider::supportsModelListing() const
{
    return true;
}

void OpenAIResponsesProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
    LLMCore::ContextData context,
    LLMCore::RequestType type,
    bool isToolsEnabled,
    bool isThinkingEnabled)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(
            QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
    }

    prompt->prepareRequest(request, context);

    auto applyModelParams = [&request](const auto &settings) {
        request["max_output_tokens"] = settings.maxTokens();

        if (settings.useTopP()) {
            request["top_p"] = settings.topP();
        }
    };

    auto applyThinkingMode = [&request](const auto &settings) {
        QString effortStr = settings.openAIResponsesReasoningEffort.stringValue().toLower();
        if (effortStr.isEmpty()) {
            effortStr = "medium";
        }

        QJsonObject reasoning;
        reasoning["effort"] = effortStr;
        request["reasoning"] = reasoning;
        request["max_output_tokens"] = settings.thinkingMaxTokens();
        request["store"] = true;

        QJsonArray include;
        include.append("reasoning.encrypted_content");
        request["include"] = include;
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == LLMCore::RequestType::QuickRefactoring) {
        const auto &qrSettings = Settings::quickRefactorSettings();
        applyModelParams(qrSettings);

        if (isThinkingEnabled) {
            applyThinkingMode(qrSettings);
        }
    } else {
        const auto &chatSettings = Settings::chatAssistantSettings();
        applyModelParams(chatSettings);

        if (isThinkingEnabled) {
            applyThinkingMode(chatSettings);
        }
    }

    if (isToolsEnabled) {
        const LLMCore::RunToolsFilter filter = (type == LLMCore::RequestType::QuickRefactoring)
                                                   ? LLMCore::RunToolsFilter::OnlyRead
                                                   : LLMCore::RunToolsFilter::ALL;

        const auto toolsDefinitions
            = m_client->tools()->getToolsDefinitions(LLMCore::ToolSchemaFormat::OpenAI, filter);
        if (!toolsDefinitions.isEmpty()) {
            QJsonArray responsesTools;

            for (const QJsonValue &toolValue : toolsDefinitions) {
                const QJsonObject tool = toolValue.toObject();
                if (tool.contains("function")) {
                    const QJsonObject functionObj = tool["function"].toObject();
                    QJsonObject responsesTool;
                    responsesTool["type"] = "function";
                    responsesTool["name"] = functionObj["name"];
                    responsesTool["description"] = functionObj["description"];
                    responsesTool["parameters"] = functionObj["parameters"];
                    responsesTools.append(responsesTool);
                }
            }
            request["tools"] = responsesTools;
        }
    }

    request["stream"] = true;
}

QFuture<QList<QString>> OpenAIResponsesProvider::getInstalledModels(const QString &url)
{

    return m_client->listModels(url);
}

QList<QString> OpenAIResponsesProvider::validateRequest(
    const QJsonObject &request, LLMCore::TemplateType type)
{
    Q_UNUSED(type);

    QList<QString> errors;

    if (!request.contains("input")) {
        errors.append("Missing required field: input");
        return errors;
    }

    const QJsonValue inputValue = request["input"];
    if (!inputValue.isString() && !inputValue.isArray()) {
        errors.append("Field 'input' must be either a string or an array");
    }

    if (request.contains("max_output_tokens") && !request["max_output_tokens"].isDouble()) {
        errors.append("Field 'max_output_tokens' must be a number");
    }

    if (request.contains("top_p") && !request["top_p"].isDouble()) {
        errors.append("Field 'top_p' must be a number");
    }

    if (request.contains("reasoning") && !request["reasoning"].isObject()) {
        errors.append("Field 'reasoning' must be an object");
    }

    if (request.contains("stream") && !request["stream"].isBool()) {
        errors.append("Field 'stream' must be a boolean");
    }

    if (request.contains("tools") && !request["tools"].isArray()) {
        errors.append("Field 'tools' must be an array");
    }

    return errors;
}

QString OpenAIResponsesProvider::apiKey() const
{
    return Settings::providerSettings().openAiApiKey();
}

LLMCore::ProviderID OpenAIResponsesProvider::providerID() const
{
    return LLMCore::ProviderID::OpenAIResponses;
}

void OpenAIResponsesProvider::sendRequest(
    const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{

    m_client->sendMessage(requestId, url, payload);
}

LLMCore::ProviderCapabilities OpenAIResponsesProvider::capabilities() const
{
    return LLMCore::ToolsCapability | LLMCore::ThinkingCapability | LLMCore::ImageCapability;
}

void OpenAIResponsesProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    m_client->cancelRequest(requestId);
}

LLMCore::ToolsManager *OpenAIResponsesProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
