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
#include <LLMCore/ToolsManager.hpp>
#include "tools/ToolsRegistration.hpp"

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
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMCore::OpenAIResponsesClient(url(), apiKey(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
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

void OpenAIResponsesProvider::prepareRequest(
    QJsonObject &request,
    PluginLLMCore::PromptTemplate *prompt,
    PluginLLMCore::ContextData context,
    PluginLLMCore::RequestType type,
    bool isToolsEnabled,
    bool isThinkingEnabled)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
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

    if (type == PluginLLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == PluginLLMCore::RequestType::QuickRefactoring) {
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
        const auto toolsDefinitions
            = m_client->tools()->getToolsDefinitions();
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

QFuture<QList<QString>> OpenAIResponsesProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(apiKey());
    return m_client->listModels().then([](const QList<QString> &models) {
        QList<QString> filtered;
        static const QStringList modelPrefixes = {"gpt-5", "o1", "o2", "o3", "o4"};
        for (const QString &modelId : models) {
            for (const QString &prefix : modelPrefixes) {
                if (modelId.contains(prefix)) {
                    filtered.append(modelId);
                    break;
                }
            }
        }
        return filtered;
    });
}

QString OpenAIResponsesProvider::apiKey() const
{
    return Settings::providerSettings().openAiApiKey();
}

void OpenAIResponsesProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }
}

PluginLLMCore::ProviderID OpenAIResponsesProvider::providerID() const
{
    return PluginLLMCore::ProviderID::OpenAIResponses;
}

PluginLLMCore::ProviderCapabilities OpenAIResponsesProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Thinking
           | PluginLLMCore::ProviderCapability::Image
           | PluginLLMCore::ProviderCapability::ModelListing;
}

::LLMCore::BaseClient *OpenAIResponsesProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
