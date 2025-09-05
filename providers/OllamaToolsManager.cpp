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

#include "OllamaToolsManager.hpp"

#include "logger/Logger.hpp"
#include <QJsonDocument>
#include <QRandomGenerator>

namespace QodeAssist::Providers {

OllamaToolsManager::OllamaToolsManager(QObject *parent)
    : QObject(parent)
    , m_toolHandler(std::make_unique<ClaudeToolHandler>(this))
{
    connect(
        m_toolHandler.get(),
        &ClaudeToolHandler::toolCompleted,
        this,
        &OllamaToolsManager::onToolCompleted);
    connect(
        m_toolHandler.get(),
        &ClaudeToolHandler::toolFailed,
        this,
        &OllamaToolsManager::onToolFailed);
}

void OllamaToolsManager::setToolsFactory(Tools::ToolsFactory *toolsFactory)
{
    m_toolsFactory = toolsFactory;
    m_toolHandler->setToolsFactory(toolsFactory);
}

QString OllamaToolsManager::processEvent(const QString &requestId, const QJsonObject &chunk)
{
    if (!m_requestStates.contains(requestId)) {
        return QString();
    }

    RequestState &state = m_requestStates[requestId];

    // Check if this chunk contains a message
    if (!chunk.contains("message")) {
        return QString();
    }

    QJsonObject message = chunk["message"].toObject();
    QString textResponse;

    // Handle text content
    if (message.contains("content")) {
        QString content = message["content"].toString();
        if (!content.isEmpty()) {
            state.assistantText += content;
            textResponse = content;
        }
    }

    // Handle tool calls (Ollama format)
    if (message.contains("tool_calls")) {
        QJsonArray toolCalls = message["tool_calls"].toArray();

        for (const QJsonValue &callValue : toolCalls) {
            QJsonObject call = callValue.toObject();

            if (!call.contains("function")) {
                continue;
            }

            QJsonObject function = call["function"].toObject();
            QString toolName = function["name"].toString();
            QJsonObject arguments = function["arguments"].toObject();

            if (toolName.isEmpty()) {
                continue;
            }

            // Generate unique ID for tool call
            QString toolId = QString::number(QRandomGenerator::global()->generate());

            ToolCall toolCall;
            toolCall.id = toolId;
            toolCall.name = toolName;
            toolCall.arguments = arguments;

            state.toolCalls.append(toolCall);
            state.pendingToolIds.enqueue(toolId);

            LOG_MESSAGE(QString("Ollama tool call received: %1 (ID: %2)").arg(toolName, toolId));
        }

        state.toolCallsReceived = true;
    }

    // Check for completion (done: true in Ollama)
    if (chunk["done"].toBool()) {
        if (state.toolCallsReceived && !state.toolCalls.isEmpty()) {
            // Start processing tools sequentially
            processNextTool(requestId);
        }
    }

    return textResponse;
}

void OllamaToolsManager::initializeRequest(
    const QString &requestId, const QJsonObject &originalRequest)
{
    m_requestStates.insert(requestId, RequestState(originalRequest));
}

void OllamaToolsManager::cleanupRequest(const QString &requestId)
{
    m_requestStates.remove(requestId);
    m_toolHandler->cleanupRequest(requestId);
}

bool OllamaToolsManager::hasToolsSupport() const
{
    return m_toolsFactory != nullptr;
}

bool OllamaToolsManager::hasActiveTools(const QString &requestId) const
{
    if (!m_requestStates.contains(requestId)) {
        return false;
    }
    return m_requestStates[requestId].hasActiveTools();
}

QJsonArray OllamaToolsManager::getToolsDefinitions() const
{
    if (!m_toolsFactory) {
        return QJsonArray();
    }

    // Convert Claude format to OpenAI format (which Ollama supports)
    QJsonArray claudeTools = m_toolsFactory->getToolsDefinitions();
    QJsonArray ollamaTools;

    for (const QJsonValue &toolValue : claudeTools) {
        QJsonObject claudeTool = toolValue.toObject();

        QJsonObject ollamaTool;
        ollamaTool["type"] = "function";

        QJsonObject function;
        function["name"] = claudeTool["name"];
        function["description"] = claudeTool["description"];
        function["parameters"] = claudeTool["input_schema"];

        ollamaTool["function"] = function;
        ollamaTools.append(ollamaTool);
    }

    return ollamaTools;
}

void OllamaToolsManager::processNextTool(const QString &requestId)
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
        sendContinuationRequest(requestId);
        return;
    }

    // Get next tool to execute
    QString toolId = state.pendingToolIds.dequeue();
    state.currentExecutingToolId = toolId;

    // Find the tool call by ID
    ToolCall *toolCall = nullptr;
    for (auto &call : state.toolCalls) {
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

    toolCall->isExecuted = true;
    LOG_MESSAGE(QString("Executing tool %1").arg(toolCall->name));
    executeToolCall(requestId, toolId, toolCall->name, toolCall->arguments);
}

void OllamaToolsManager::executeToolCall(
    const QString &requestId,
    const QString &toolId,
    const QString &toolName,
    const QJsonObject &input)
{
    m_toolHandler->executeTool(requestId, toolId, toolName, input);
}

void OllamaToolsManager::onToolCompleted(
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

void OllamaToolsManager::onToolFailed(
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

void OllamaToolsManager::sendContinuationRequest(const QString &requestId)
{
    if (!m_requestStates.contains(requestId)) {
        return;
    }

    const RequestState &state = m_requestStates[requestId];

    // Create Ollama format follow-up request
    QJsonObject newRequest = state.originalRequest;
    QJsonArray messages = state.originalMessages;

    // Add assistant message with tool calls
    QJsonObject assistantMessage;
    assistantMessage["role"] = "assistant";

    // Add text content if present
    if (!state.assistantText.isEmpty()) {
        assistantMessage["content"] = state.assistantText;
    } else {
        assistantMessage["content"] = "";
    }

    // Add tool calls in Ollama format
    QJsonArray toolCalls;
    for (const auto &call : state.toolCalls) {
        QJsonObject toolCall;
        QJsonObject function;
        function["name"] = call.name;
        function["arguments"] = call.arguments;
        toolCall["function"] = function;
        toolCalls.append(toolCall);
    }

    if (!toolCalls.isEmpty()) {
        assistantMessage["tool_calls"] = toolCalls;
    }

    messages.append(assistantMessage);

    // Add tool result messages (Ollama uses "tool" role)
    for (const auto &call : state.toolCalls) {
        if (state.toolResults.contains(call.id)) {
            QJsonObject toolMessage;
            toolMessage["role"] = "tool";
            toolMessage["content"] = state.toolResults[call.id];
            // Ollama might expect tool name in message
            toolMessage["name"] = call.name;
            messages.append(toolMessage);
        }
    }

    newRequest["messages"] = messages;
    emit requestReadyForContinuation(requestId, newRequest);
}

} // namespace QodeAssist::Providers
