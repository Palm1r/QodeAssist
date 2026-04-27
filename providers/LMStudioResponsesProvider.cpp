// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LMStudioResponsesProvider.hpp"

#include <LLMQore/ToolsManager.hpp>

#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "tools/ToolsRegistration.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QodeAssist::Providers {

LMStudioResponsesProvider::LMStudioResponsesProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMQore::OpenAIResponsesClient(QString(), QString(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString LMStudioResponsesProvider::name() const
{
    return "LM Studio (Responses API)";
}

QString LMStudioResponsesProvider::apiKey() const
{
    return {};
}

QString LMStudioResponsesProvider::url() const
{
    return "http://localhost:1234";
}

void LMStudioResponsesProvider::prepareRequest(
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
        const auto toolsDefinitions = m_client->tools()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to LM Studio Responses request")
                            .arg(toolsDefinitions.size()));
        }
    }

    request["stream"] = true;
}

QFuture<QList<QString>> LMStudioResponsesProvider::getInstalledModels(const QString &baseUrl)
{
    QString url = baseUrl;
    if (!url.endsWith(QStringLiteral("/v1")))
        url += QStringLiteral("/v1");
    m_client->setUrl(url);
    m_client->setApiKey(apiKey());
    return m_client->listModels();
}

PluginLLMCore::RequestID LMStudioResponsesProvider::sendRequest(
    const QUrl &url, const QJsonObject &payload, const QString &endpoint)
{
    const QString effectiveEndpoint
        = endpoint.isEmpty() ? QStringLiteral("/v1/responses") : endpoint;
    return PluginLLMCore::Provider::sendRequest(url, payload, effectiveEndpoint);
}

PluginLLMCore::ProviderID LMStudioResponsesProvider::providerID() const
{
    return PluginLLMCore::ProviderID::OpenAIResponses;
}

PluginLLMCore::ProviderCapabilities LMStudioResponsesProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Thinking
           | PluginLLMCore::ProviderCapability::Image
           | PluginLLMCore::ProviderCapability::ModelListing;
}

::LLMQore::BaseClient *LMStudioResponsesProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
