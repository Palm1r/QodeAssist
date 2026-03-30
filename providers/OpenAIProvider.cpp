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

#include "OpenAIProvider.hpp"

#include <LLMCore/ToolsManager.hpp>
#include "tools/ToolsRegistration.hpp"
#include "pluginllmcore/ValidationUtils.hpp"
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

OpenAIProvider::OpenAIProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMCore::OpenAIClient(url(), apiKey(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString OpenAIProvider::name() const
{
    return "OpenAI";
}

QString OpenAIProvider::url() const
{
    return "https://api.openai.com";
}

QString OpenAIProvider::completionEndpoint() const
{
    return "/v1/chat/completions";
}

QString OpenAIProvider::chatEndpoint() const
{
    return "/v1/chat/completions";
}

bool OpenAIProvider::supportsModelListing() const
{
    return true;
}

void OpenAIProvider::prepareRequest(
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
        QString model = request.value("model").toString().toLower();
        bool useNewParameter = model.contains("gpt-4o") || model.contains("gpt-4-turbo")
                               || model.contains("o1-") || model.contains("gpt-5")
                               || model.startsWith("o1") || model.contains("o3");

        bool isReasoningModel = model.contains("o1-") || model.contains("gpt-5")
                                || model.startsWith("o1") || model.contains("o3");

        if (useNewParameter) {
            request["max_completion_tokens"] = settings.maxTokens();
        } else {
            request["max_tokens"] = settings.maxTokens();
        }

        if (!isReasoningModel) {
            request["temperature"] = settings.temperature();

            if (settings.useTopP())
                request["top_p"] = settings.topP();
            if (settings.useTopK())
                request["top_k"] = settings.topK();

        } else {
            request["temperature"] = 1.0;
        }

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
            LOG_MESSAGE(QString("Added %1 tools to OpenAI request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> OpenAIProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(apiKey());
    return m_client->listModels().then([](const QList<QString> &allModels) {
        QList<QString> filtered;
        for (const QString &modelId : allModels) {
            if (!modelId.contains("dall-e") && !modelId.contains("whisper")
                && !modelId.contains("tts") && !modelId.contains("davinci")
                && !modelId.contains("babbage") && !modelId.contains("omni")) {
                filtered.append(modelId);
            }
        }
        return filtered;
    });
}

QList<QString> OpenAIProvider::validateRequest(const QJsonObject &request, PluginLLMCore::TemplateType type)
{
    const auto templateReq = QJsonObject{
        {"model", {}},
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
        {"temperature", {}},
        {"max_tokens", {}},
        {"max_completion_tokens", {}}, // New parameter for newer models
        {"top_p", {}},
        {"top_k", {}},
        {"frequency_penalty", {}},
        {"presence_penalty", {}},
        {"stop", QJsonArray{}},
        {"stream", {}},
        {"tools", {}}};

    return PluginLLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString OpenAIProvider::apiKey() const
{
    return Settings::providerSettings().openAiApiKey();
}

void OpenAIProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }
}

PluginLLMCore::ProviderID OpenAIProvider::providerID() const
{
    return PluginLLMCore::ProviderID::OpenAI;
}

void OpenAIProvider::sendRequest(
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

    LOG_MESSAGE(QString("OpenAIProvider: Sending request %1 (client: %2) to %3")
                    .arg(requestId, clientId, url.toString()));
}

bool OpenAIProvider::supportsTools() const
{
    return true;
}

bool OpenAIProvider::supportImage() const
{
    return true;
}

void OpenAIProvider::cancelRequest(const PluginLLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("OpenAIProvider: Cancelling request %1").arg(requestId));

    if (m_providerToClientIds.contains(requestId)) {
        auto clientId = m_providerToClientIds.take(requestId);
        m_clientToProviderIds.remove(clientId);
        m_client->cancelRequest(clientId);
    }
    m_awaitingContinuation.remove(requestId);
}

::LLMCore::ToolsManager *OpenAIProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
