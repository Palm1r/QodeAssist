// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ClaudeProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <LLMQore/ToolsManager.hpp>

#include "ClaudeCacheControl.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"
#include "tools/ToolsRegistration.hpp"

namespace QodeAssist::Providers {

ClaudeProvider::ClaudeProvider(QObject *parent)
    : Providers::Provider(parent)
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
    Templates::PromptTemplate *prompt,
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
        request["max_tokens"] = settings.maxTokens();
        if (settings.useTopP())
            request["top_p"] = settings.topP();
        if (settings.useTopK())
            request["top_k"] = settings.topK();
        request["stream"] = true;
    };

    auto applyThinkingMode = [&request](const auto &settings) {
        const QString model = request.value("model").toString().toLower();
        const bool useAdaptiveThinking = model.contains("opus-4-8") || model.contains("opus-4-7")
                                         || model.contains("opus-4-6") || model.contains("sonnet-4-6");

        QJsonObject thinkingObj;
        if (useAdaptiveThinking) {
            thinkingObj["type"] = "adaptive";

            const int budget = settings.thinkingBudgetTokens();
            QString effort = "high";
            if (budget < 8000)
                effort = "low";
            else if (budget < 24000)
                effort = "medium";

            QJsonObject outputConfig;
            outputConfig["effort"] = effort;
            request["output_config"] = outputConfig;
        } else {
            thinkingObj["type"] = "enabled";
            thinkingObj["budget_tokens"] = settings.thinkingBudgetTokens();
        }
        request["thinking"] = thinkingObj;
        request["max_tokens"] = settings.thinkingMaxTokens();
        request["temperature"] = 1.0;
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
        request["temperature"] = Settings::codeCompletionSettings().temperature();
    } else if (type == LLMCore::RequestType::QuickRefactoring) {
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

    const auto &ps = Settings::providerSettings();
    const bool cachingOn = ps.claudeEnablePromptCaching()
                           && type != LLMCore::RequestType::CodeCompletion;
    m_client->setUseExtendedCacheTTL(cachingOn && ps.claudeUseExtendedCacheTTL());
    if (cachingOn) {
        ClaudeCacheControl::apply(request, ps.claudeUseExtendedCacheTTL());
    }
}

QFuture<QList<QString>> ClaudeProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(apiKey());
    return m_client->listModels();
}

Providers::ProviderID ClaudeProvider::providerID() const
{
    return Providers::ProviderID::Claude;
}

Providers::ProviderCapabilities ClaudeProvider::capabilities() const
{
    return Providers::ProviderCapability::Tools | Providers::ProviderCapability::Thinking
           | Providers::ProviderCapability::Image
           | Providers::ProviderCapability::ModelListing;
}

::LLMQore::BaseClient *ClaudeProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
