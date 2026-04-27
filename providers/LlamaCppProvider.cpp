// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LlamaCppProvider.hpp"

#include <LLMQore/ToolsManager.hpp>
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"
#include "tools/ToolsRegistration.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QodeAssist::Providers {

LlamaCppProvider::LlamaCppProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMQore::LlamaCppClient(QString(), QString(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString LlamaCppProvider::name() const
{
    return "llama.cpp";
}

QString LlamaCppProvider::apiKey() const
{
    return Settings::providerSettings().llamaCppApiKey();
}

QString LlamaCppProvider::url() const
{
    return "http://localhost:8080";
}

void LlamaCppProvider::prepareRequest(
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

    auto applyThinkingMode = [&request]() {
        QJsonObject chatTemplateKwargs = request["chat_template_kwargs"].toObject();
        chatTemplateKwargs["enable_thinking"] = true;
        request["chat_template_kwargs"] = chatTemplateKwargs;
    };

    if (type == PluginLLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == PluginLLMCore::RequestType::QuickRefactoring) {
        applyModelParams(Settings::quickRefactorSettings());
        if (isThinkingEnabled) {
            applyThinkingMode();
            LOG_MESSAGE(QString("LlamaCppProvider: Thinking mode enabled for QuickRefactoring"));
        }
    } else {
        applyModelParams(Settings::chatAssistantSettings());
        if (isThinkingEnabled) {
            applyThinkingMode();
            LOG_MESSAGE(QString("LlamaCppProvider: Thinking mode enabled for Chat"));
        }
    }

    if (isToolsEnabled) {
        auto toolsDefinitions = m_client->tools()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to llama.cpp request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> LlamaCppProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(Settings::providerSettings().llamaCppApiKey());
    return m_client->listModels();
}

PluginLLMCore::ProviderID LlamaCppProvider::providerID() const
{
    return PluginLLMCore::ProviderID::LlamaCpp;
}

PluginLLMCore::ProviderCapabilities LlamaCppProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Thinking
           | PluginLLMCore::ProviderCapability::Image
           | PluginLLMCore::ProviderCapability::ModelListing;
}

::LLMQore::BaseClient *LlamaCppProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
