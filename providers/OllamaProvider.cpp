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

#include "OllamaProvider.hpp"

#include <LLMQore/ToolsManager.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"
#include "tools/ToolsRegistration.hpp"

namespace QodeAssist::Providers {

OllamaProvider::OllamaProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMQore::OllamaClient(QString(), QString(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString OllamaProvider::name() const
{
    return "Ollama";
}

QString OllamaProvider::apiKey() const
{
    return Settings::providerSettings().ollamaBasicAuthApiKey();
}

QString OllamaProvider::url() const
{
    return "http://localhost:11434";
}

void OllamaProvider::prepareRequest(
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

    auto applyThinkingMode = [&request]() {
        request["enable_thinking"] = true;
        QJsonObject options = request["options"].toObject();
        options["temperature"] = 1.0;
        request["options"] = options;
    };

    if (type == PluginLLMCore::RequestType::CodeCompletion) {
        applySettings(Settings::codeCompletionSettings());
    } else if (type == PluginLLMCore::RequestType::QuickRefactoring) {
        const auto &qrSettings = Settings::quickRefactorSettings();
        applySettings(qrSettings);

        if (isThinkingEnabled) {
            applyThinkingMode();
            LOG_MESSAGE(QString("OllamaProvider: Thinking mode enabled for QuickRefactoring"));
        }
    } else {
        const auto &chatSettings = Settings::chatAssistantSettings();
        applySettings(chatSettings);

        if (isThinkingEnabled) {
            applyThinkingMode();
            LOG_MESSAGE(QString("OllamaProvider: Thinking mode enabled for Chat"));
        }
    }

    if (isToolsEnabled) {
        auto toolsDefinitions = m_client->tools()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(
                QString("OllamaProvider: Added %1 tools to request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> OllamaProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(Settings::providerSettings().ollamaBasicAuthApiKey());
    return m_client->listModels();
}

PluginLLMCore::ProviderID OllamaProvider::providerID() const
{
    return PluginLLMCore::ProviderID::Ollama;
}

PluginLLMCore::ProviderCapabilities OllamaProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Thinking
           | PluginLLMCore::ProviderCapability::Image
           | PluginLLMCore::ProviderCapability::ModelListing;
}

PluginLLMCore::RequestID OllamaProvider::sendRequest(
    const QUrl &url,
    const QJsonObject &payload,
    PluginLLMCore::RequestType type,
    const QString &endpointOverride)
{
    const QString endpoint = !endpointOverride.isEmpty()
                                 ? endpointOverride
                                 : (type == PluginLLMCore::RequestType::CodeCompletion
                                        ? QStringLiteral("/api/generate")
                                        : QString());
    return PluginLLMCore::Provider::sendRequest(url, payload, type, endpoint);
}

::LLMQore::BaseClient *OllamaProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
