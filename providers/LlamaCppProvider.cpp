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

#include "LlamaCppProvider.hpp"

#include "llmcore/OpenAIClient.hpp"
#include "tools/ToolsFactory.hpp"

#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"

#include <QJsonArray>
#include <QJsonObject>

namespace QodeAssist::Providers {

LlamaCppProvider::LlamaCppProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_client(new LLMCore::OpenAIClient(this))
{
    Tools::registerAppTools(m_client->tools());
    Tools::configureToolSettings(m_client->tools());
    m_client->setAuthHeaderName("");
    connectClientSignals(m_client);
}

QString LlamaCppProvider::name() const
{
    return "llama.cpp";
}

QString LlamaCppProvider::url() const
{
    return "http://localhost:8080";
}

QString LlamaCppProvider::completionEndpoint() const
{
    return "/infill";
}

QString LlamaCppProvider::chatEndpoint() const
{
    return "/v1/chat/completions";
}

bool LlamaCppProvider::supportsModelListing() const
{
    return false;
}

void LlamaCppProvider::prepareRequest(
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
        LLMCore::RunToolsFilter filter = LLMCore::RunToolsFilter::ALL;
        if (type == LLMCore::RequestType::QuickRefactoring) {
            filter = LLMCore::RunToolsFilter::OnlyRead;
        }

        auto toolsDefinitions = m_client->tools()->getToolsDefinitions(
            LLMCore::ToolSchemaFormat::OpenAI, filter);
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to llama.cpp request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> LlamaCppProvider::getInstalledModels(const QString &)
{
    return QtFuture::makeReadyFuture(QList<QString>{});
}

QList<QString> LlamaCppProvider::validateRequest(
    const QJsonObject &request, LLMCore::TemplateType type)
{
    if (type == LLMCore::TemplateType::FIM) {
        const auto infillReq = QJsonObject{
            {"model", {}},
            {"input_prefix", {}},
            {"input_suffix", {}},
            {"input_extra", {}},
            {"prompt", {}},
            {"temperature", {}},
            {"top_p", {}},
            {"top_k", {}},
            {"max_tokens", {}},
            {"frequency_penalty", {}},
            {"presence_penalty", {}},
            {"stop", QJsonArray{}},
            {"stream", {}}};

        return LLMCore::ValidationUtils::validateRequestFields(request, infillReq);
    } else {
        const auto chatReq = QJsonObject{
            {"model", {}},
            {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
            {"temperature", {}},
            {"max_tokens", {}},
            {"top_p", {}},
            {"top_k", {}},
            {"frequency_penalty", {}},
            {"presence_penalty", {}},
            {"stop", QJsonArray{}},
            {"stream", {}},
            {"tools", {}}};

        return LLMCore::ValidationUtils::validateRequestFields(request, chatReq);
    }
}

QString LlamaCppProvider::apiKey() const
{
    return {};
}

LLMCore::ProviderID LlamaCppProvider::providerID() const
{
    return LLMCore::ProviderID::LlamaCpp;
}

void LlamaCppProvider::sendRequest(
    const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    m_client->sendMessage(requestId, url, payload);
}

LLMCore::ProviderCapabilities LlamaCppProvider::capabilities() const
{
    return LLMCore::ToolsCapability | LLMCore::ImageCapability;
}

void LlamaCppProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    m_client->cancelRequest(requestId);
}

LLMCore::ToolsManager *LlamaCppProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
