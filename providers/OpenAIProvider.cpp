// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "OpenAIProvider.hpp"

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

OpenAIProvider::OpenAIProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMQore::OpenAIClient(QString(), QString(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString OpenAIProvider::name() const
{
    return "OpenAI (Chat Completions)";
}

QString OpenAIProvider::apiKey() const
{
    return Settings::providerSettings().openAiApiKey();
}

QString OpenAIProvider::url() const
{
    return "https://api.openai.com/v1";
}

void OpenAIProvider::prepareRequest(
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
            LOG_MESSAGE(QString("Added %1 tools to OpenAI request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> OpenAIProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(apiKey());
    return m_client->listModels().then([](const QList<QString> &allModels) {
        QList<QString> filtered;
        for (const QString &modelId : allModels) {
            if (!modelId.contains("dall-e") && !modelId.contains("whisper")
                && !modelId.contains("tts") && !modelId.contains("davinci")
                && !modelId.contains("babbage") && !modelId.contains("omni")) {
                filtered.append(modelId);
            }
        }
        return filtered;
    });
}

PluginLLMCore::ProviderID OpenAIProvider::providerID() const
{
    return PluginLLMCore::ProviderID::OpenAI;
}

PluginLLMCore::ProviderCapabilities OpenAIProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Image
           | PluginLLMCore::ProviderCapability::ModelListing;
}

::LLMQore::BaseClient *OpenAIProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
