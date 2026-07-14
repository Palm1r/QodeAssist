// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "OllamaCompatProvider.hpp"

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

OllamaCompatProvider::OllamaCompatProvider(QObject *parent)
    : Providers::Provider(parent)
    , m_client(new ::LLMQore::OpenAIClient(QString(), QString(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString OllamaCompatProvider::name() const
{
    return "Ollama (OpenAI-compatible)";
}

QString OllamaCompatProvider::apiKey() const
{
    return Settings::providerSettings().ollamaBasicAuthApiKey();
}

QString OllamaCompatProvider::url() const
{
    return "http://localhost:11434";
}

void OllamaCompatProvider::prepareRequest(
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

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == LLMCore::RequestType::QuickRefactoring) {
        applyModelParams(Settings::quickRefactorSettings());
    } else {
        applyModelParams(Settings::chatAssistantSettings());
    }

    if (isToolsEnabled) {
        auto toolsDefinitions = m_client->tools()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(
                QString("Added %1 tools to OllamaCompat request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> OllamaCompatProvider::getInstalledModels(const QString &baseUrl)
{
    QString url = baseUrl;
    if (!url.endsWith(QStringLiteral("/v1")))
        url += QStringLiteral("/v1");
    m_client->setUrl(url);
    m_client->setApiKey(apiKey());
    return m_client->listModels();
}

LLMQore::RequestID OllamaCompatProvider::sendRequest(
    const QUrl &url, const QJsonObject &payload, const QString &endpoint)
{
    const QString effectiveEndpoint
        = endpoint.isEmpty() ? QStringLiteral("/v1/chat/completions") : endpoint;
    return Providers::Provider::sendRequest(url, payload, effectiveEndpoint);
}

Providers::ProviderID OllamaCompatProvider::providerID() const
{
    return Providers::ProviderID::OpenAICompatible;
}

Providers::ProviderCapabilities OllamaCompatProvider::capabilities() const
{
    return Providers::ProviderCapability::Tools | Providers::ProviderCapability::Image
           | Providers::ProviderCapability::ModelListing
           | Providers::ProviderCapability::Thinking;
}

::LLMQore::BaseClient *OllamaCompatProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
