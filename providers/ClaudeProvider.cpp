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

#include "ClaudeProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <LLMQore/ToolsManager.hpp>

#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"
#include "tools/ToolsRegistration.hpp"

namespace QodeAssist::Providers {

ClaudeProvider::ClaudeProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMQore::ClaudeClient(QString(), QString(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString ClaudeProvider::name() const
{
    return "Claude";
}

QString ClaudeProvider::apiKey() const
{
    return Settings::providerSettings().claudeApiKey();
}

QString ClaudeProvider::url() const
{
    return "https://api.anthropic.com";
}

void ClaudeProvider::prepareRequest(
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
        request["max_tokens"] = settings.maxTokens();
        if (settings.useTopP())
            request["top_p"] = settings.topP();
        if (settings.useTopK())
            request["top_k"] = settings.topK();
        request["stream"] = true;
    };

    auto applyThinkingMode = [&request](const auto &settings) {
        QJsonObject thinkingObj;
        thinkingObj["type"] = "enabled";
        thinkingObj["budget_tokens"] = settings.thinkingBudgetTokens();
        request["thinking"] = thinkingObj;
        request["max_tokens"] = settings.thinkingMaxTokens();
        request["temperature"] = 1.0;
    };

    if (type == PluginLLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
        request["temperature"] = Settings::codeCompletionSettings().temperature();
    } else if (type == PluginLLMCore::RequestType::QuickRefactoring) {
        const auto &qrSettings = Settings::quickRefactorSettings();
        applyModelParams(qrSettings);

        if (isThinkingEnabled) {
            applyThinkingMode(qrSettings);
        } else {
            request["temperature"] = qrSettings.temperature();
        }
    } else {
        const auto &chatSettings = Settings::chatAssistantSettings();
        applyModelParams(chatSettings);

        if (isThinkingEnabled) {
            applyThinkingMode(chatSettings);
        } else {
            request["temperature"] = chatSettings.temperature();
        }
    }

    if (isToolsEnabled) {
        auto toolsDefinitions = m_client->tools()->getToolsDefinitions();

        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to Claude request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> ClaudeProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(apiKey());
    return m_client->listModels();
}

PluginLLMCore::ProviderID ClaudeProvider::providerID() const
{
    return PluginLLMCore::ProviderID::Claude;
}

PluginLLMCore::ProviderCapabilities ClaudeProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Thinking
           | PluginLLMCore::ProviderCapability::Image
           | PluginLLMCore::ProviderCapability::ModelListing;
}

::LLMQore::BaseClient *ClaudeProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
