// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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
    : Providers::Provider(parent)
    , m_client(new ::LLMQore::OllamaClient(QString(), QString(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString OllamaProvider::name() const
{
    return "Ollama (Native)";
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

    if (type == LLMCore::RequestType::CodeCompletion) {
        applySettings(Settings::codeCompletionSettings());
    } else if (type == LLMCore::RequestType::QuickRefactoring) {
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

Providers::ProviderID OllamaProvider::providerID() const
{
    return Providers::ProviderID::Ollama;
}

Providers::ProviderCapabilities OllamaProvider::capabilities() const
{
    return Providers::ProviderCapability::Tools | Providers::ProviderCapability::Thinking
           | Providers::ProviderCapability::Image
           | Providers::ProviderCapability::ModelListing;
}

::LLMQore::BaseClient *OllamaProvider::client() const
{
    return m_client;
}

} // namespace QodeAssist::Providers
