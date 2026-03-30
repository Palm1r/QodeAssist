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

#include "ClaudeProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <LLMCore/ToolsManager.hpp>

#include "pluginllmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"
#include "tools/ToolsRegistration.hpp"

namespace QodeAssist::Providers {

ClaudeProvider::ClaudeProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMCore::ClaudeClient(
          url(), Settings::providerSettings().claudeApiKey(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
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

    if (type == PluginLLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
        request["temperature"] = Settings::codeCompletionSettings().temperature();
    } else if (type == PluginLLMCore::RequestType::QuickRefactoring) {
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
}

QFuture<QList<QString>> ClaudeProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(apiKey());
    return m_client->listModels();
}

QList<QString> ClaudeProvider::validateRequest(
    const QJsonObject &request, PluginLLMCore::TemplateType type)
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

    return PluginLLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString ClaudeProvider::apiKey() const
{
    return Settings::providerSettings().claudeApiKey();
}

void ClaudeProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("anthropic-version", "2023-06-01");

    if (!apiKey().isEmpty()) {
        networkRequest.setRawHeader("x-api-key", apiKey().toUtf8());
    }
}

PluginLLMCore::ProviderID ClaudeProvider::providerID() const
{
    return PluginLLMCore::ProviderID::Claude;
}

void ClaudeProvider::sendRequest(
    const PluginLLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    QUrl baseUrl(url);
    baseUrl.setPath("");
    m_client->setUrl(baseUrl.toString());
    m_client->setApiKey(apiKey());

    ::LLMCore::RequestCallbacks callbacks;

    callbacks.onChunk = [this, requestId](const ::LLMCore::RequestID &, const QString &chunk) {
        if (m_awaitingContinuation.remove(requestId)) {
            emit continuationStarted(requestId);
        }
        emit partialResponseReceived(requestId, chunk);
    };

    callbacks.onCompleted
        = [this, requestId](const ::LLMCore::RequestID &clientId, const QString &fullText) {
              emit fullResponseReceived(requestId, fullText);
              m_providerToClientIds.remove(requestId);
              m_clientToProviderIds.remove(clientId);
              m_awaitingContinuation.remove(requestId);
          };

    callbacks.onFailed
        = [this, requestId](const ::LLMCore::RequestID &clientId, const QString &error) {
              emit requestFailed(requestId, error);
              m_providerToClientIds.remove(requestId);
              m_clientToProviderIds.remove(clientId);
              m_awaitingContinuation.remove(requestId);
          };

    callbacks.onThinkingBlock = [this, requestId](const ::LLMCore::RequestID &,
                                                   const QString &thinking,
                                                   const QString &signature) {
        if (m_awaitingContinuation.remove(requestId)) {
            emit continuationStarted(requestId);
        }
        if (thinking.isEmpty()) {
            emit redactedThinkingBlockReceived(requestId, signature);
        } else {
            emit thinkingBlockReceived(requestId, thinking, signature);
        }
    };

    callbacks.onToolStarted = [this, requestId](const ::LLMCore::RequestID &,
                                                 const QString &toolId,
                                                 const QString &toolName) {
        emit toolExecutionStarted(requestId, toolId, toolName);
        m_awaitingContinuation.insert(requestId);
    };

    callbacks.onToolResult = [this, requestId](const ::LLMCore::RequestID &,
                                               const QString &toolId,
                                               const QString &toolName,
                                               const QString &result) {
        emit toolExecutionCompleted(requestId, toolId, toolName, result);
    };

    auto clientId = m_client->sendMessage(payload, callbacks);
    m_providerToClientIds[requestId] = clientId;
    m_clientToProviderIds[clientId] = requestId;

    LOG_MESSAGE(QString("ClaudeProvider: Sending request %1 (client: %2) to %3")
                    .arg(requestId, clientId, url.toString()));
}

bool ClaudeProvider::supportsTools() const
{
    return true;
}

bool ClaudeProvider::supportThinking() const
{
    return true;
}

bool ClaudeProvider::supportImage() const
{
    return true;
}

void ClaudeProvider::cancelRequest(const PluginLLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("ClaudeProvider: Cancelling request %1").arg(requestId));

    if (m_providerToClientIds.contains(requestId)) {
        auto clientId = m_providerToClientIds.take(requestId);
        m_clientToProviderIds.remove(clientId);
        m_client->cancelRequest(clientId);
    }
    m_awaitingContinuation.remove(requestId);
}

::LLMCore::ToolsManager *ClaudeProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
