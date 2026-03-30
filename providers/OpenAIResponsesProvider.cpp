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

#include "OpenAIResponsesProvider.hpp"
#include <LLMCore/ToolsManager.hpp>
#include "tools/ToolsRegistration.hpp"

#include "pluginllmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QodeAssist::Providers {

OpenAIResponsesProvider::OpenAIResponsesProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMCore::OpenAIResponsesClient(url(), apiKey(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());
}

QString OpenAIResponsesProvider::name() const
{
    return "OpenAI Responses";
}

QString OpenAIResponsesProvider::url() const
{
    return "https://api.openai.com";
}

QString OpenAIResponsesProvider::completionEndpoint() const
{
    return "/v1/responses";
}

QString OpenAIResponsesProvider::chatEndpoint() const
{
    return "/v1/responses";
}

bool OpenAIResponsesProvider::supportsModelListing() const
{
    return true;
}

void OpenAIResponsesProvider::prepareRequest(
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
        request["max_output_tokens"] = settings.maxTokens();

        if (settings.useTopP()) {
            request["top_p"] = settings.topP();
        }
    };

    auto applyThinkingMode = [&request](const auto &settings) {
        QString effortStr = settings.openAIResponsesReasoningEffort.stringValue().toLower();
        if (effortStr.isEmpty()) {
            effortStr = "medium";
        }

        QJsonObject reasoning;
        reasoning["effort"] = effortStr;
        request["reasoning"] = reasoning;
        request["max_output_tokens"] = settings.thinkingMaxTokens();
        request["store"] = true;

        QJsonArray include;
        include.append("reasoning.encrypted_content");
        request["include"] = include;
    };

    if (type == PluginLLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == PluginLLMCore::RequestType::QuickRefactoring) {
        const auto &qrSettings = Settings::quickRefactorSettings();
        applyModelParams(qrSettings);

        if (isThinkingEnabled) {
            applyThinkingMode(qrSettings);
        }
    } else {
        const auto &chatSettings = Settings::chatAssistantSettings();
        applyModelParams(chatSettings);

        if (isThinkingEnabled) {
            applyThinkingMode(chatSettings);
        }
    }

    if (isToolsEnabled) {
        const auto toolsDefinitions
            = m_client->tools()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            QJsonArray responsesTools;

            for (const QJsonValue &toolValue : toolsDefinitions) {
                const QJsonObject tool = toolValue.toObject();
                if (tool.contains("function")) {
                    const QJsonObject functionObj = tool["function"].toObject();
                    QJsonObject responsesTool;
                    responsesTool["type"] = "function";
                    responsesTool["name"] = functionObj["name"];
                    responsesTool["description"] = functionObj["description"];
                    responsesTool["parameters"] = functionObj["parameters"];
                    responsesTools.append(responsesTool);
                }
            }
            request["tools"] = responsesTools;
        }
    }

    request["stream"] = true;
}

QFuture<QList<QString>> OpenAIResponsesProvider::getInstalledModels(const QString &baseUrl)
{
    m_client->setUrl(baseUrl);
    m_client->setApiKey(apiKey());
    return m_client->listModels().then([](const QList<QString> &models) {
        QList<QString> filtered;
        static const QStringList modelPrefixes = {"gpt-5", "o1", "o2", "o3", "o4"};
        for (const QString &modelId : models) {
            for (const QString &prefix : modelPrefixes) {
                if (modelId.contains(prefix)) {
                    filtered.append(modelId);
                    break;
                }
            }
        }
        return filtered;
    });
}

QList<QString> OpenAIResponsesProvider::validateRequest(
    const QJsonObject &request, PluginLLMCore::TemplateType type)
{
    Q_UNUSED(type);

    QList<QString> errors;

    if (!request.contains("input")) {
        errors.append("Missing required field: input");
        return errors;
    }

    const QJsonValue inputValue = request["input"];
    if (!inputValue.isString() && !inputValue.isArray()) {
        errors.append("Field 'input' must be either a string or an array");
    }

    if (request.contains("max_output_tokens") && !request["max_output_tokens"].isDouble()) {
        errors.append("Field 'max_output_tokens' must be a number");
    }

    if (request.contains("top_p") && !request["top_p"].isDouble()) {
        errors.append("Field 'top_p' must be a number");
    }

    if (request.contains("reasoning") && !request["reasoning"].isObject()) {
        errors.append("Field 'reasoning' must be an object");
    }

    if (request.contains("stream") && !request["stream"].isBool()) {
        errors.append("Field 'stream' must be a boolean");
    }

    if (request.contains("tools") && !request["tools"].isArray()) {
        errors.append("Field 'tools' must be an array");
    }

    return errors;
}

QString OpenAIResponsesProvider::apiKey() const
{
    return Settings::providerSettings().openAiApiKey();
}

void OpenAIResponsesProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }
}

PluginLLMCore::ProviderID OpenAIResponsesProvider::providerID() const
{
    return PluginLLMCore::ProviderID::OpenAIResponses;
}

void OpenAIResponsesProvider::sendRequest(
    const PluginLLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    QUrl baseUrl(url);
    baseUrl.setPath("");
    m_client->setUrl(baseUrl.toString());
    m_client->setApiKey(apiKey());

    ::LLMCore::RequestCallbacks callbacks;

    callbacks.onChunk = [this, requestId](const ::LLMCore::RequestID &, const QString &chunk) {
        if (m_awaitingContinuation.remove(requestId))
            emit continuationStarted(requestId);
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
        if (m_awaitingContinuation.remove(requestId))
            emit continuationStarted(requestId);
        if (thinking.isEmpty())
            emit redactedThinkingBlockReceived(requestId, signature);
        else
            emit thinkingBlockReceived(requestId, thinking, signature);
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

    LOG_MESSAGE(QString("OpenAIResponsesProvider: Sending request %1 (client: %2) to %3")
                    .arg(requestId, clientId, url.toString()));
}

bool OpenAIResponsesProvider::supportsTools() const
{
    return true;
}

bool OpenAIResponsesProvider::supportImage() const
{
    return true;
}

bool OpenAIResponsesProvider::supportThinking() const
{
    return true;
}

void OpenAIResponsesProvider::cancelRequest(const PluginLLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("OpenAIResponsesProvider: Cancelling request %1").arg(requestId));

    if (m_providerToClientIds.contains(requestId)) {
        auto clientId = m_providerToClientIds.take(requestId);
        m_clientToProviderIds.remove(clientId);
        m_client->cancelRequest(clientId);
    }
    m_awaitingContinuation.remove(requestId);
}

::LLMCore::ToolsManager *OpenAIResponsesProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
