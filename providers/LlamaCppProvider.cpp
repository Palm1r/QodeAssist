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

#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace QodeAssist::Providers {

LlamaCppProvider::LlamaCppProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_toolsManager(new Tools::ToolsManager(this))
{
    connect(
        m_toolsManager,
        &Tools::ToolsManager::toolExecutionComplete,
        this,
        &LlamaCppProvider::onToolExecutionComplete);
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
        auto toolsDefinitions = m_toolsManager->getToolsDefinitions(
            LLMCore::ToolSchemaFormat::OpenAI);
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to llama.cpp request").arg(toolsDefinitions.size()));
        }
    }
}

QList<QString> LlamaCppProvider::getInstalledModels(const QString &url)
{
    return {};
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

void LlamaCppProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
}

LLMCore::ProviderID LlamaCppProvider::providerID() const
{
    return LLMCore::ProviderID::LlamaCpp;
}

void LlamaCppProvider::sendRequest(
    const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    if (!m_messages.contains(requestId)) {
        m_dataBuffers[requestId].clear();
    }

    m_requestUrls[requestId] = url;
    m_originalRequests[requestId] = payload;

    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = payload};

    LOG_MESSAGE(
        QString("LlamaCppProvider: Sending request %1 to %2").arg(requestId, url.toString()));

    emit httpClient()->sendRequest(request);
}

bool LlamaCppProvider::supportsTools() const
{
    return true;
}

void LlamaCppProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("LlamaCppProvider: Cancelling request %1").arg(requestId));
    LLMCore::Provider::cancelRequest(requestId);
    cleanupRequest(requestId);
}

void LlamaCppProvider::onDataReceived(
    const QodeAssist::LLMCore::RequestID &requestId, const QByteArray &data)
{
    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty() || line == "data: [DONE]") {
            continue;
        }

        QJsonObject chunk = parseEventLine(line);
        if (chunk.isEmpty())
            continue;

        if (chunk.contains("content")) {
            QString content = chunk["content"].toString();
            if (!content.isEmpty()) {
                buffers.responseContent += content;
                emit partialResponseReceived(requestId, content);
            }
            if (chunk["stop"].toBool()) {
                emit fullResponseReceived(requestId, buffers.responseContent);
                m_dataBuffers.remove(requestId);
            }
        } else if (chunk.contains("choices")) {
            processStreamChunk(requestId, chunk);
        }
    }
}

void LlamaCppProvider::onRequestFinished(
    const QodeAssist::LLMCore::RequestID &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("LlamaCppProvider request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
        cleanupRequest(requestId);
        return;
    }

    if (m_messages.contains(requestId)) {
        OpenAIMessage *message = m_messages[requestId];
        if (message->state() == LLMCore::MessageState::RequiresToolExecution) {
            LOG_MESSAGE(QString("Waiting for tools to complete for %1").arg(requestId));
            m_dataBuffers.remove(requestId);
            return;
        }
    }

    if (m_dataBuffers.contains(requestId)) {
        const LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
        if (!buffers.responseContent.isEmpty()) {
            LOG_MESSAGE(QString("Emitting full response for %1").arg(requestId));
            emit fullResponseReceived(requestId, buffers.responseContent);
        }
    }

    cleanupRequest(requestId);
}

void LlamaCppProvider::onToolExecutionComplete(
    const QString &requestId, const QHash<QString, QString> &toolResults)
{
    if (!m_messages.contains(requestId) || !m_requestUrls.contains(requestId)) {
        LOG_MESSAGE(QString("ERROR: Missing data for continuation request %1").arg(requestId));
        cleanupRequest(requestId);
        return;
    }

    LOG_MESSAGE(QString("Tool execution complete for llama.cpp request %1").arg(requestId));

    for (auto it = toolResults.begin(); it != toolResults.end(); ++it) {
        OpenAIMessage *message = m_messages[requestId];
        auto toolContent = message->getCurrentToolUseContent();
        for (auto tool : toolContent) {
            if (tool->id() == it.key()) {
                auto toolStringName = m_toolsManager->toolsFactory()->getStringName(tool->name());
                emit toolExecutionCompleted(
                    requestId, tool->id(), toolStringName, toolResults[tool->id()]);
                break;
            }
        }
    }

    OpenAIMessage *message = m_messages[requestId];
    QJsonObject continuationRequest = m_originalRequests[requestId];
    QJsonArray messages = continuationRequest["messages"].toArray();

    messages.append(message->toProviderFormat());

    QJsonArray toolResultMessages = message->createToolResultMessages(toolResults);
    for (const auto &toolMsg : toolResultMessages) {
        messages.append(toolMsg);
    }

    continuationRequest["messages"] = messages;

    LOG_MESSAGE(QString("Sending continuation request for %1 with %2 tool results")
                    .arg(requestId)
                    .arg(toolResults.size()));

    sendRequest(requestId, m_requestUrls[requestId], continuationRequest);
}

void LlamaCppProvider::processStreamChunk(const QString &requestId, const QJsonObject &chunk)
{
    QJsonArray choices = chunk["choices"].toArray();
    if (choices.isEmpty()) {
        return;
    }

    QJsonObject choice = choices[0].toObject();
    QJsonObject delta = choice["delta"].toObject();
    QString finishReason = choice["finish_reason"].toString();

    OpenAIMessage *message = m_messages.value(requestId);
    if (!message) {
        message = new OpenAIMessage(this);
        m_messages[requestId] = message;
        LOG_MESSAGE(QString("Created NEW OpenAIMessage for llama.cpp request %1").arg(requestId));

        if (m_dataBuffers.contains(requestId)) {
            emit continuationStarted(requestId);
            LOG_MESSAGE(QString("Starting continuation for request %1").arg(requestId));
        }
    } else if (
        m_dataBuffers.contains(requestId)
        && message->state() == LLMCore::MessageState::RequiresToolExecution) {
        message->startNewContinuation();
        emit continuationStarted(requestId);
        LOG_MESSAGE(QString("Cleared message state for continuation request %1").arg(requestId));
    }

    if (delta.contains("content") && !delta["content"].isNull()) {
        QString content = delta["content"].toString();
        message->handleContentDelta(content);

        LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
        buffers.responseContent += content;
        emit partialResponseReceived(requestId, content);
    }

    if (delta.contains("tool_calls")) {
        QJsonArray toolCalls = delta["tool_calls"].toArray();
        for (const auto &toolCallValue : toolCalls) {
            QJsonObject toolCall = toolCallValue.toObject();
            int index = toolCall["index"].toInt();

            if (toolCall.contains("id")) {
                QString id = toolCall["id"].toString();
                QJsonObject function = toolCall["function"].toObject();
                QString name = function["name"].toString();
                message->handleToolCallStart(index, id, name);
            }

            if (toolCall.contains("function")) {
                QJsonObject function = toolCall["function"].toObject();
                if (function.contains("arguments")) {
                    QString args = function["arguments"].toString();
                    message->handleToolCallDelta(index, args);
                }
            }
        }
    }

    if (!finishReason.isEmpty() && finishReason != "null") {
        for (int i = 0; i < 10; ++i) {
            message->handleToolCallComplete(i);
        }

        message->handleFinishReason(finishReason);
        handleMessageComplete(requestId);
    }
}

void LlamaCppProvider::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    OpenAIMessage *message = m_messages[requestId];

    if (message->state() == LLMCore::MessageState::RequiresToolExecution) {
        LOG_MESSAGE(QString("llama.cpp message requires tool execution for %1").arg(requestId));

        auto toolUseContent = message->getCurrentToolUseContent();

        if (toolUseContent.isEmpty()) {
            LOG_MESSAGE(QString("No tools to execute for %1").arg(requestId));
            return;
        }

        for (auto toolContent : toolUseContent) {
            auto toolStringName = m_toolsManager->toolsFactory()->getStringName(toolContent->name());
            emit toolExecutionStarted(requestId, toolContent->id(), toolStringName);
            m_toolsManager->executeToolCall(
                requestId, toolContent->id(), toolContent->name(), toolContent->input());
        }

    } else {
        LOG_MESSAGE(QString("llama.cpp message marked as complete for %1").arg(requestId));
    }
}

void LlamaCppProvider::cleanupRequest(const LLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("Cleaning up llama.cpp request %1").arg(requestId));

    if (m_messages.contains(requestId)) {
        OpenAIMessage *message = m_messages.take(requestId);
        message->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);
    m_toolsManager->cleanupRequest(requestId);
}

} // namespace QodeAssist::Providers
