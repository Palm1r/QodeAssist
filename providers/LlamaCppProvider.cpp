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

#include <LLMCore/ToolsManager.hpp>
#include "pluginllmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "tools/ToolsRegistration.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QodeAssist::Providers {

LlamaCppProvider::LlamaCppProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMCore::LlamaCppClient(url(), apiKey(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
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
            LOG_MESSAGE(QString("Added %1 tools to llama.cpp request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> LlamaCppProvider::getInstalledModels(const QString &)
{
    return QtFuture::makeReadyFuture(QList<QString>{});
}

QList<QString> LlamaCppProvider::validateRequest(
    const QJsonObject &request, PluginLLMCore::TemplateType type)
{
    if (type == PluginLLMCore::TemplateType::FIM) {
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

        return PluginLLMCore::ValidationUtils::validateRequestFields(request, infillReq);
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

        return PluginLLMCore::ValidationUtils::validateRequestFields(request, chatReq);
    }
}

QString LlamaCppProvider::apiKey() const
{
    return {};
}

void LlamaCppProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
}

PluginLLMCore::ProviderID LlamaCppProvider::providerID() const
{
    return PluginLLMCore::ProviderID::LlamaCpp;
}

void LlamaCppProvider::sendRequest(
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

    LOG_MESSAGE(QString("LlamaCppProvider: Sending request %1 (client: %2) to %3")
                    .arg(requestId, clientId, url.toString()));
}

PluginLLMCore::ProviderCapabilities LlamaCppProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Image;
}

void LlamaCppProvider::cancelRequest(const PluginLLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("LlamaCppProvider: Cancelling request %1").arg(requestId));

    if (m_providerToClientIds.contains(requestId)) {
        auto clientId = m_providerToClientIds.take(requestId);
        m_clientToProviderIds.remove(clientId);
        m_client->cancelRequest(clientId);
    }
    m_awaitingContinuation.remove(requestId);
}

::LLMCore::ToolsManager *LlamaCppProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
