// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "GoogleAIProvider.hpp"

#include <LLMQore/ToolsManager.hpp>

#include <QJsonArray>
#include "tools/ToolsRegistration.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QtCore/qurlquery.h>

#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

GoogleAIProvider::GoogleAIProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMQore::GoogleAIClient(QString(), QString(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString GoogleAIProvider::name() const
{
    return "Google AI";
}

QString GoogleAIProvider::apiKey() const
{
    return Settings::providerSettings().googleAiApiKey();
}

QString GoogleAIProvider::url() const
{
    return "https://generativelanguage.googleapis.com/v1beta";
}

void GoogleAIProvider::prepareRequest(
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
        QJsonObject generationConfig;
        generationConfig["maxOutputTokens"] = settings.maxTokens();
        generationConfig["temperature"] = settings.temperature();

        if (settings.useTopP())
            generationConfig["topP"] = settings.topP();
        if (settings.useTopK())
            generationConfig["topK"] = settings.topK();

        request["generationConfig"] = generationConfig;
    };

    auto applyThinkingMode = [&request](const auto &settings) {
        QJsonObject generationConfig;
        generationConfig["maxOutputTokens"] = settings.thinkingMaxTokens();

        if (settings.useTopP())
            generationConfig["topP"] = settings.topP();
        if (settings.useTopK())
            generationConfig["topK"] = settings.topK();

        generationConfig["temperature"] = 1.0;

        QJsonObject thinkingConfig;
        thinkingConfig["includeThoughts"] = true;
        int budgetTokens = settings.thinkingBudgetTokens();
        if (budgetTokens != -1) {
            thinkingConfig["thinkingBudget"] = budgetTokens;
        }

        generationConfig["thinkingConfig"] = thinkingConfig;
        request["generationConfig"] = generationConfig;
    };

    if (type == PluginLLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == PluginLLMCore::RequestType::QuickRefactoring) {
        const auto &qrSettings = Settings::quickRefactorSettings();

        if (isThinkingEnabled) {
            applyThinkingMode(qrSettings);
        } else {
            applyModelParams(qrSettings);
        }
    } else {
        const auto &chatSettings = Settings::chatAssistantSettings();

        if (isThinkingEnabled) {
            applyThinkingMode(chatSettings);
        } else {
            applyModelParams(chatSettings);
        }
    }

    if (isToolsEnabled) {
        auto toolsDefinitions = m_client->tools()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to Google AI request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> GoogleAIProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(apiKey());
    return m_client->listModels();
}

PluginLLMCore::ProviderID GoogleAIProvider::providerID() const
{
    return PluginLLMCore::ProviderID::GoogleAI;
}

PluginLLMCore::ProviderCapabilities GoogleAIProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Thinking
           | PluginLLMCore::ProviderCapability::Image
           | PluginLLMCore::ProviderCapability::ModelListing;
}

PluginLLMCore::RequestID GoogleAIProvider::sendRequest(
    const QUrl &url, const QJsonObject &payload, const QString &endpoint)
{
    // Gemini takes the model from the URL path and streaming from the
    // action suffix (:streamGenerateContent vs :generateContent), and
    // rejects unknown top-level body fields. The shared call-site seeds
    // payload with {model, stream}; consume them here into client state
    // before they hit the wire.
    QJsonObject cleaned = payload;
    if (cleaned.contains("model")) {
        m_client->setModel(cleaned["model"].toString());
        cleaned.remove("model");
    }
    cleaned.remove("stream");

    return PluginLLMCore::Provider::sendRequest(url, cleaned, endpoint);
}

::LLMQore::BaseClient *GoogleAIProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
