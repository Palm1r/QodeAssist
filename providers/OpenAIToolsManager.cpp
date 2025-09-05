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

#include "OpenAIToolsManager.hpp"

#include "logger/Logger.hpp"
#include <QJsonDocument>

namespace QodeAssist::Providers {

OpenAIToolsManager::OpenAIToolsManager(QObject *parent)
    : QObject(parent)
    , m_toolHandler(std::make_unique<ClaudeToolHandler>(this))
{
    connect(
        m_toolHandler.get(),
        &ClaudeToolHandler::toolCompleted,
        this,
        &OpenAIToolsManager::onToolCompleted);
    connect(
        m_toolHandler.get(),
        &ClaudeToolHandler::toolFailed,
        this,
        &OpenAIToolsManager::onToolFailed);
}

void OpenAIToolsManager::setToolsFactory(Tools::ToolsFactory *toolsFactory)
{
    m_toolsFactory = toolsFactory;
    m_toolHandler->setToolsFactory(toolsFactory);
}

QString OpenAIToolsManager::processEvent(const QString &requestId, const QJsonObject &chunk)
{
    if (!m_requestStates.contains(requestId)) {
        return QString();
    }

    RequestState &state = m_requestStates[requestId];

    // OpenAI format: chunk.choices[0].delta
    QJsonArray choices = chunk["choices"].toArray();
    if (choices.isEmpty()) {
        return QString();
    }

    QJsonObject delta = choices[0].toObject()["delta"].toObject();
    QString textResponse;

    // Handle text content
    if (delta.contains("content")) {
        QString content = delta["content"].toString();
        if (!content.isNull()) {
            state.assistantText += content;
            textResponse = content;
        }
    }

    // Handle tool calls
    if (delta.contains("tool_calls")) {
        QJsonArray toolCalls = delta["tool_calls"].toArray();

        for (const QJsonValue &callValue : toolCalls) {
            QJsonObject call = callValue.toObject();
            if (!call.contains("index")) {
                continue; // Skip malformed tool calls
            }

            int index = call["index"].toInt();

            // Initialize or update tool call
            if (!state.activeCalls.contains(index)) {
                state.activeCalls[index] = ToolCall();
            }

            ToolCall &toolCall = state.activeCalls[index];

            if (call.contains("id") && !call["id"].toString().isEmpty()) {
                toolCall.id = call["id"].toString();
            }

            if (call.contains("function")) {
                QJsonObject function = call["function"].toObject();

                if (function.contains("name") && !function["name"].toString().isEmpty()) {
                    toolCall.name = function["name"].toString();
                    LOG_MESSAGE(
                        QString("Tool call started: %1 (ID: %2)").arg(toolCall.name, toolCall.id));
                }

                if (function.contains("arguments")) {
                    QString args = function["arguments"].toString();
                    if (!args.isNull()) {
                        toolCall.arguments += args;
                    }
                }
            }
        }
    }

    // Check for completion - OpenAI sends finish_reason
    QJsonObject choice = choices[0].toObject();
    QString finishReason = choice["finish_reason"].toString();

    if (finishReason == "tool_calls") {
        state.allToolsReceived = true;

        // Queue all completed tool calls for sequential execution
        for (auto it = state.activeCalls.begin(); it != state.activeCalls.end(); ++it) {
            ToolCall &toolCall = it.value();
            if (!toolCall.isComplete && !toolCall.id.isEmpty() && !toolCall.name.isEmpty()
                && !toolCall.arguments.isEmpty()) {
                QJsonParseError parseError;
                QJsonDocument argsDoc
                    = QJsonDocument::fromJson(toolCall.arguments.toUtf8(), &parseError);

                if (parseError.error == QJsonParseError::NoError) {
                    toolCall.isComplete = true;
                    state.pendingToolIds.enqueue(toolCall.id);
                    LOG_MESSAGE(QString("Queued tool %1 for execution").arg(toolCall.name));
                } else {
                    LOG_MESSAGE(QString("Invalid JSON arguments for tool %1: %2")
                                    .arg(toolCall.name, parseError.errorString()));
                    // Mark as failed
                    toolCall.isComplete = true;
                    state.toolResults[toolCall.id] = QString("Error: Invalid JSON arguments - %1")
                                                         .arg(parseError.errorString());
                }
            }
        }

        // Start processing tools sequentially
        processNextTool(requestId);
    }

    return textResponse;
}

void OpenAIToolsManager::initializeRequest(
    const QString &requestId, const QJsonObject &originalRequest)
{
    m_requestStates.insert(requestId, RequestState(originalRequest));
}

void OpenAIToolsManager::cleanupRequest(const QString &requestId)
{
    m_requestStates.remove(requestId);
    m_toolHandler->cleanupRequest(requestId);
}

bool OpenAIToolsManager::hasToolsSupport() const
{
    return m_toolsFactory != nullptr;
}

bool OpenAIToolsManager::hasActiveTools(const QString &requestId) const
{
    if (!m_requestStates.contains(requestId)) {
        return false;
    }
    return m_requestStates[requestId].hasActiveTools();
}

QJsonArray OpenAIToolsManager::getToolsDefinitions() const
{
    if (!m_toolsFactory) {
        return QJsonArray();
    }

    // Convert Claude format to OpenAI format
    QJsonArray claudeTools = m_toolsFactory->getToolsDefinitions();
    QJsonArray openaiTools;

    for (const QJsonValue &toolValue : claudeTools) {
        QJsonObject claudeTool = toolValue.toObject();

        QJsonObject openaiTool;
        openaiTool["type"] = "function";

        QJsonObject function;
        function["name"] = claudeTool["name"];
        function["description"] = claudeTool["description"];
        function["parameters"] = claudeTool["input_schema"];

        openaiTool["function"] = function;
        openaiTools.append(openaiTool);
    }

    return openaiTools;
}

void OpenAIToolsManager::processNextTool(const QString &requestId)
{
    if (!m_requestStates.contains(requestId)) {
        return;
    }

    RequestState &state = m_requestStates[requestId];

    // If already executing a tool, wait for completion
    if (!state.currentExecutingToolId.isEmpty()) {
        return;
    }

    // If no more tools to execute, send continuation request
    if (state.pendingToolIds.isEmpty()) {
        if (state.allToolsReceived) {
            sendContinuationRequest(requestId);
        }
        return;
    }

    // Get next tool to execute
    QString toolId = state.pendingToolIds.dequeue();
    state.currentExecutingToolId = toolId;

    // Find the tool call by ID
    ToolCall *toolCall = nullptr;
    for (auto &call : state.activeCalls) {
        if (call.id == toolId) {
            toolCall = &call;
            break;
        }
    }

    if (!toolCall || toolCall->isExecuted) {
        LOG_MESSAGE(QString("Tool call not found or already executed: %1").arg(toolId));
        state.currentExecutingToolId.clear();
        processNextTool(requestId); // Try next tool
        return;
    }

    // Parse arguments and execute tool
    QJsonParseError parseError;
    QJsonDocument argsDoc = QJsonDocument::fromJson(toolCall->arguments.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        LOG_MESSAGE(QString("Failed to parse tool arguments for %1: %2")
                        .arg(toolCall->name, parseError.errorString()));
        state.toolResults[toolId]
            = QString("Error: Invalid arguments - %1").arg(parseError.errorString());
        state.currentExecutingToolId.clear();
        processNextTool(requestId); // Try next tool
        return;
    }

    toolCall->isExecuted = true;
    LOG_MESSAGE(QString("Executing tool %1").arg(toolCall->name));
    executeToolCall(requestId, toolId, toolCall->name, argsDoc.object());
}

void OpenAIToolsManager::executeToolCall(
    const QString &requestId,
    const QString &toolId,
    const QString &toolName,
    const QJsonObject &input)
{
    m_toolHandler->executeTool(requestId, toolId, toolName, input);
}

void OpenAIToolsManager::onToolCompleted(
    const QString &requestId, const QString &toolId, const QString &result)
{
    if (!m_requestStates.contains(requestId)) {
        LOG_MESSAGE(QString("No request state found for completed tool %1").arg(toolId));
        return;
    }

    RequestState &state = m_requestStates[requestId];
    state.toolResults[toolId] = result;
    state.currentExecutingToolId.clear();

    LOG_MESSAGE(QString("Tool %1 completed for request %2").arg(toolId, requestId));

    // Process next tool in queue
    processNextTool(requestId);
}

void OpenAIToolsManager::onToolFailed(
    const QString &requestId, const QString &toolId, const QString &error)
{
    if (!m_requestStates.contains(requestId)) {
        LOG_MESSAGE(QString("No request state found for failed tool %1").arg(toolId));
        return;
    }

    RequestState &state = m_requestStates[requestId];
    state.toolResults[toolId] = QString("Error: %1").arg(error);
    state.currentExecutingToolId.clear();

    LOG_MESSAGE(QString("Tool %1 failed for request %2: %3").arg(toolId, requestId, error));

    // Process next tool in queue (continue despite failure)
    processNextTool(requestId);
}

void OpenAIToolsManager::sendContinuationRequest(const QString &requestId)
{
    if (!m_requestStates.contains(requestId)) {
        return;
    }

    const RequestState &state = m_requestStates[requestId];

    // Create OpenAI format follow-up request
    QJsonObject newRequest = state.originalRequest;
    QJsonArray messages = state.originalMessages;

    // Add assistant message with tool calls
    QJsonObject assistantMessage;
    assistantMessage["role"] = "assistant";
    if (!state.assistantText.isEmpty()) {
        assistantMessage["content"] = state.assistantText;
    } else {
        assistantMessage["content"] = QJsonValue(); // null
    }

    QJsonArray toolCalls;
    for (const auto &call : state.activeCalls) {
        if (call.isComplete) {
            QJsonObject toolCall;
            toolCall["id"] = call.id;
            toolCall["type"] = "function";

            QJsonObject function;
            function["name"] = call.name;
            function["arguments"] = call.arguments;
            toolCall["function"] = function;

            toolCalls.append(toolCall);
        }
    }
    assistantMessage["tool_calls"] = toolCalls;
    messages.append(assistantMessage);

    // Add tool result messages in the order they were executed
    for (const auto &call : state.activeCalls) {
        if (call.isComplete && state.toolResults.contains(call.id)) {
            QJsonObject toolMessage;
            toolMessage["role"] = "tool";
            toolMessage["tool_call_id"] = call.id;
            toolMessage["content"] = state.toolResults[call.id];
            messages.append(toolMessage);
        }
    }

    newRequest["messages"] = messages;
    emit requestReadyForContinuation(requestId, newRequest);
}

} // namespace QodeAssist::Providers
