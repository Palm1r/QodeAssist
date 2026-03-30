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

#include "OpenAICompatProvider.hpp"
#include <LLMCore/ToolsManager.hpp>

#include "pluginllmcore/ValidationUtils.hpp"
#include "tools/ToolsRegistration.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QodeAssist::Providers {

OpenAICompatProvider::OpenAICompatProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMCore::OpenAIClient(url(), apiKey(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString OpenAICompatProvider::name() const
{
    return "OpenAI Compatible";
}

QString OpenAICompatProvider::url() const
{
    return "http://localhost:1234";
}

QString OpenAICompatProvider::completionEndpoint() const
{
    return "/v1/chat/completions";
}

QString OpenAICompatProvider::chatEndpoint() const
{
    return "/v1/chat/completions";
}

bool OpenAICompatProvider::supportsModelListing() const
{
    return false;
}

void OpenAICompatProvider::prepareRequest(
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
            LOG_MESSAGE(
                QString("Added %1 tools to OpenAICompat request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> OpenAICompatProvider::getInstalledModels(const QString &)
{
    return QtFuture::makeReadyFuture(QList<QString>{});
}

QList<QString> OpenAICompatProvider::validateRequest(
    const QJsonObject &request, PluginLLMCore::TemplateType type)
{
    const auto templateReq = QJsonObject{
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

    return PluginLLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString OpenAICompatProvider::apiKey() const
{
    return Settings::providerSettings().openAiCompatApiKey();
}

void OpenAICompatProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }
}

PluginLLMCore::ProviderID OpenAICompatProvider::providerID() const
{
    return PluginLLMCore::ProviderID::OpenAICompatible;
}

void OpenAICompatProvider::sendRequest(
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

    LOG_MESSAGE(QString("OpenAICompatProvider: Sending request %1 (client: %2) to %3")
                    .arg(requestId, clientId, url.toString()));
}

bool OpenAICompatProvider::supportsTools() const
{
    return true;
}

bool OpenAICompatProvider::supportImage() const
{
    return true;
}

void OpenAICompatProvider::cancelRequest(const PluginLLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("OpenAICompatProvider: Cancelling request %1").arg(requestId));

    if (m_providerToClientIds.contains(requestId)) {
        auto clientId = m_providerToClientIds.take(requestId);
        m_clientToProviderIds.remove(clientId);
        m_client->cancelRequest(clientId);
    }
    m_awaitingContinuation.remove(requestId);
}

::LLMCore::ToolsManager *OpenAICompatProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
