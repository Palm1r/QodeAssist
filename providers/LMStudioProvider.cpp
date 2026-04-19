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

#include "LMStudioProvider.hpp"

#include <LLMQore/ToolsManager.hpp>

#include "tools/ToolsRegistration.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QodeAssist::Providers {

LMStudioProvider::LMStudioProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMQore::OpenAIClient(QString(), QString(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString LMStudioProvider::name() const
{
    return "LM Studio";
}

QString LMStudioProvider::apiKey() const
{
    return {};
}

QString LMStudioProvider::url() const
{
    return "http://localhost:1234/v1";
}

QFuture<QList<QString>> LMStudioProvider::getInstalledModels(const QString &url)
{
    m_client->setUrl(url);
    m_client->setApiKey(apiKey());
    return m_client->listModels();
}

PluginLLMCore::ProviderID LMStudioProvider::providerID() const
{
    return PluginLLMCore::ProviderID::LMStudio;
}

PluginLLMCore::ProviderCapabilities LMStudioProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Image
           | PluginLLMCore::ProviderCapability::ModelListing;
}

void LMStudioProvider::prepareRequest(
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

    if (type == PluginLLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == PluginLLMCore::RequestType::QuickRefactoring) {
        applyModelParams(Settings::quickRefactorSettings());
    } else {
        applyModelParams(Settings::chatAssistantSettings());
    }

    if (isToolsEnabled) {
        auto toolsDefinitions = m_client->tools()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to LMStudio request").arg(toolsDefinitions.size()));
        }
    }
}

::LLMQore::BaseClient *LMStudioProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
