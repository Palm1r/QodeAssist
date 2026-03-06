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

#include <llmcore/clients/ollama/OllamaClient.hpp>
#include "tools/ToolsFactory.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <llmcore/prompt/ValidationUtils.hpp>
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

OllamaProvider::OllamaProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_client(new LLMCore::OllamaClient([this]() { return apiKey(); }, this))
{
    Tools::registerAppTools(m_client->tools());
    Tools::configureToolSettings(m_client->tools());
    connectClientSignals(m_client);
}

QString OllamaProvider::name() const
{
    return "Ollama";
}

QString OllamaProvider::url() const
{
    return "http://localhost:11434";
}

QString OllamaProvider::completionEndpoint() const
{
    return "/api/generate";
}

QString OllamaProvider::chatEndpoint() const
{
    return "/api/chat";
}

bool OllamaProvider::supportsModelListing() const
{
    return true;
}

void OllamaProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
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
        LLMCore::RunToolsFilter filter = LLMCore::RunToolsFilter::ALL;
        if (type == LLMCore::RequestType::QuickRefactoring) {
            filter = LLMCore::RunToolsFilter::OnlyRead;
        }

        auto toolsDefinitions = m_client->tools()->getToolsDefinitions(
            LLMCore::ToolSchemaFormat::Ollama, filter);
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(
                QString("OllamaProvider: Added %1 tools to request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> OllamaProvider::getInstalledModels(const QString &url)
{

    return m_client->listModels(url);
}

QList<QString> OllamaProvider::validateRequest(const QJsonObject &request, LLMCore::TemplateType type)
{
    const auto fimReq = QJsonObject{
        {"keep_alive", {}},
        {"model", {}},
        {"stream", {}},
        {"prompt", {}},
        {"suffix", {}},
        {"system", {}},
        {"images", QJsonArray{}},
        {"options",
         QJsonObject{
             {"temperature", {}},
             {"stop", {}},
             {"top_p", {}},
             {"top_k", {}},
             {"num_predict", {}},
             {"frequency_penalty", {}},
             {"presence_penalty", {}}}}};

    const auto messageReq = QJsonObject{
        {"keep_alive", {}},
        {"model", {}},
        {"stream", {}},
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}, {"images", QJsonArray{}}}}}},
        {"tools", QJsonArray{}},
        {"options",
         QJsonObject{
             {"temperature", {}},
             {"stop", {}},
             {"top_p", {}},
             {"top_k", {}},
             {"num_predict", {}},
             {"frequency_penalty", {}},
             {"presence_penalty", {}}}}};

    return LLMCore::ValidationUtils::validateRequestFields(
        request, type == LLMCore::TemplateType::FIM ? fimReq : messageReq);
}

QString OllamaProvider::apiKey() const
{
    return Settings::providerSettings().ollamaBasicAuthApiKey();
}

LLMCore::ProviderID OllamaProvider::providerID() const
{
    return LLMCore::ProviderID::Ollama;
}

void OllamaProvider::sendRequest(
    const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{

    m_client->sendMessage(requestId, url, payload);
}

LLMCore::ProviderCapabilities OllamaProvider::capabilities() const
{
    return LLMCore::ToolsCapability | LLMCore::ThinkingCapability | LLMCore::ImageCapability;
}

void OllamaProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    m_client->cancelRequest(requestId);
}

LLMCore::ToolsManager *OllamaProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
