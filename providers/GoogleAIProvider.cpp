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

#include "GoogleAIProvider.hpp"

#include <LLMCore/ToolsManager.hpp>

#include <QJsonArray>
#include "tools/ToolsRegistration.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QtCore/qurlquery.h>

#include "pluginllmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

GoogleAIProvider::GoogleAIProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMCore::GoogleAIClient(url(), apiKey(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString GoogleAIProvider::name() const
{
    return "Google AI";
}

QString GoogleAIProvider::url() const
{
    return "https://generativelanguage.googleapis.com/v1beta";
}

QString GoogleAIProvider::completionEndpoint() const
{
    return {};
}

QString GoogleAIProvider::chatEndpoint() const
{
    return {};
}

bool GoogleAIProvider::supportsModelListing() const
{
    return true;
}

void GoogleAIProvider::prepareRequest(
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
        QJsonObject generationConfig;
        generationConfig["maxOutputTokens"] = settings.maxTokens();
        generationConfig["temperature"] = settings.temperature();

        if (settings.useTopP())
            generationConfig["topP"] = settings.topP();
        if (settings.useTopK())
            generationConfig["topK"] = settings.topK();

        request["generationConfig"] = generationConfig;
    };

    auto applyThinkingMode = [&request](const auto &settings) {
        QJsonObject generationConfig;
        generationConfig["maxOutputTokens"] = settings.thinkingMaxTokens();

        if (settings.useTopP())
            generationConfig["topP"] = settings.topP();
        if (settings.useTopK())
            generationConfig["topK"] = settings.topK();

        generationConfig["temperature"] = 1.0;

        QJsonObject thinkingConfig;
        thinkingConfig["includeThoughts"] = true;
        int budgetTokens = settings.thinkingBudgetTokens();
        if (budgetTokens != -1) {
            thinkingConfig["thinkingBudget"] = budgetTokens;
        }

        generationConfig["thinkingConfig"] = thinkingConfig;
        request["generationConfig"] = generationConfig;
    };

    if (type == PluginLLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == PluginLLMCore::RequestType::QuickRefactoring) {
        const auto &qrSettings = Settings::quickRefactorSettings();

        if (isThinkingEnabled) {
            applyThinkingMode(qrSettings);
        } else {
            applyModelParams(qrSettings);
        }
    } else {
        const auto &chatSettings = Settings::chatAssistantSettings();

        if (isThinkingEnabled) {
            applyThinkingMode(chatSettings);
        } else {
            applyModelParams(chatSettings);
        }
    }

    if (isToolsEnabled) {
        auto toolsDefinitions = m_client->tools()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to Google AI request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> GoogleAIProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(apiKey());
    return m_client->listModels();
}

QList<QString> GoogleAIProvider::validateRequest(
    const QJsonObject &request, PluginLLMCore::TemplateType type)
{
    QJsonObject templateReq;

    templateReq = QJsonObject{
        {"contents", QJsonArray{}},
        {"system_instruction", QJsonArray{}},
        {"generationConfig",
         QJsonObject{
             {"temperature", {}},
             {"maxOutputTokens", {}},
             {"topP", {}},
             {"topK", {}},
             {"thinkingConfig",
              QJsonObject{{"thinkingBudget", {}}, {"includeThoughts", {}}}}}},
        {"safetySettings", QJsonArray{}},
        {"tools", QJsonArray{}}};

    return PluginLLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString GoogleAIProvider::apiKey() const
{
    return Settings::providerSettings().googleAiApiKey();
}

void GoogleAIProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QUrl url = networkRequest.url();
    QUrlQuery query(url.query());
    query.addQueryItem("key", apiKey());
    url.setQuery(query);
    networkRequest.setUrl(url);
}

PluginLLMCore::ProviderID GoogleAIProvider::providerID() const
{
    return PluginLLMCore::ProviderID::GoogleAI;
}

void GoogleAIProvider::sendRequest(
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

    LOG_MESSAGE(QString("GoogleAIProvider: Sending request %1 (client: %2) to %3")
                    .arg(requestId, clientId, url.toString()));
}

bool GoogleAIProvider::supportsTools() const
{
    return true;
}

bool GoogleAIProvider::supportThinking() const
{
    return true;
}

bool GoogleAIProvider::supportImage() const
{
    return true;
}

void GoogleAIProvider::cancelRequest(const PluginLLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("GoogleAIProvider: Cancelling request %1").arg(requestId));

    if (m_providerToClientIds.contains(requestId)) {
        auto clientId = m_providerToClientIds.take(requestId);
        m_clientToProviderIds.remove(clientId);
        m_client->cancelRequest(clientId);
    }
    m_awaitingContinuation.remove(requestId);
}

::LLMCore::ToolsManager *GoogleAIProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
