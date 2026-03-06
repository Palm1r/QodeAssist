/*
 * Copyright (C) 2024-2026 Petr Mironychev
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

#include "ClaudeProvider.hpp"

#include <llmcore/clients/claude/ClaudeClient.hpp>
#include "tools/ToolsFactory.hpp"

#include <QJsonArray>
#include <QJsonObject>

#include <llmcore/prompt/ValidationUtils.hpp>
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"

namespace QodeAssist::Providers {

ClaudeProvider::ClaudeProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_client(new LLMCore::ClaudeClient([this]() { return apiKey(); }, this))
{
    Tools::registerAppTools(m_client->tools());
    Tools::configureToolSettings(m_client->tools());
    connectClientSignals(m_client);
}

QString ClaudeProvider::name() const
{
    return "Claude";
}

QString ClaudeProvider::url() const
{
    return "https://api.anthropic.com";
}

QString ClaudeProvider::completionEndpoint() const
{
    return "/v1/messages";
}

QString ClaudeProvider::chatEndpoint() const
{
    return "/v1/messages";
}

bool ClaudeProvider::supportsModelListing() const
{
    return true;
}

void ClaudeProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
    LLMCore::ContextData context,
    LLMCore::RequestType type,
    bool isToolsEnabled,
    bool isThinkingEnabled)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(
            QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
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
        QJsonObject thinkingObj;
        thinkingObj["type"] = "enabled";
        thinkingObj["budget_tokens"] = settings.thinkingBudgetTokens();
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
        LLMCore::RunToolsFilter filter = LLMCore::RunToolsFilter::ALL;
        if (type == LLMCore::RequestType::QuickRefactoring) {
            filter = LLMCore::RunToolsFilter::OnlyRead;
        }

        auto toolsDefinitions = m_client->tools()->getToolsDefinitions(
            LLMCore::ToolSchemaFormat::Claude, filter);
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to Claude request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> ClaudeProvider::getInstalledModels(const QString &baseUrl)
{

    return m_client->listModels(baseUrl);
}

QList<QString> ClaudeProvider::validateRequest(
    const QJsonObject &request, LLMCore::TemplateType type)
{
    const auto templateReq = QJsonObject{
        {"model", {}},
        {"system", {}},
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
        {"temperature", {}},
        {"max_tokens", {}},
        {"anthropic-version", {}},
        {"top_p", {}},
        {"top_k", {}},
        {"stop", QJsonArray{}},
        {"stream", {}},
        {"tools", {}},
        {"thinking", QJsonObject{{"type", {}}, {"budget_tokens", {}}}}};

    return LLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString ClaudeProvider::apiKey() const
{
    return Settings::providerSettings().claudeApiKey();
}

LLMCore::ProviderID ClaudeProvider::providerID() const
{
    return LLMCore::ProviderID::Claude;
}

void ClaudeProvider::sendRequest(
    const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{

    m_client->sendMessage(requestId, url, payload);
}

LLMCore::ProviderCapabilities ClaudeProvider::capabilities() const
{
    return LLMCore::ToolsCapability | LLMCore::ThinkingCapability | LLMCore::ImageCapability;
}

void ClaudeProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    m_client->cancelRequest(requestId);
}

LLMCore::ToolsManager *ClaudeProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
