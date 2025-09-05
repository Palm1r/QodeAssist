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

#include "ClaudeToolHandler.hpp"

#include <QJsonArray>
#include <QTimer>

#include <logger/Logger.hpp>

namespace QodeAssist::Providers {

ClaudeToolHandler::ClaudeToolHandler(QObject *parent)
    : QObject(parent)
{}

void ClaudeToolHandler::setToolsFactory(Tools::ToolsFactory *toolsFactory)
{
    m_toolsFactory = toolsFactory;
}

void ClaudeToolHandler::initializeRequest(const QString &requestId, const QJsonObject &request)
{
    RequestState &state = m_requestStates[requestId];
    state.originalRequest = request;
    state.assistantText.clear();
    state.clearTool();
}

void ClaudeToolHandler::clearRequest(const QString &requestId)
{
    m_requestStates.remove(requestId);
}

void ClaudeToolHandler::processToolStart(
    const QString &requestId,
    const QString &toolName,
    const QString &toolId,
    const QJsonObject &input)
{
    RequestState &state = m_requestStates[requestId];
    state.currentToolName = toolName;
    state.currentToolId = toolId;
    state.currentToolInput = input;

    LOG_MESSAGE(
        QString("ClaudeToolHandler: Received tool call %1 with id %2").arg(toolName, toolId));

    // Execute tool asynchronously
    QTimer::singleShot(0, this, [this, requestId, toolName, toolId, input]() {
        executeToolAsync(requestId, toolName, toolId, input);
    });
}

void ClaudeToolHandler::addAssistantText(const QString &requestId, const QString &text)
{
    if (m_requestStates.contains(requestId)) {
        m_requestStates[requestId].assistantText += text;
    }
}

bool ClaudeToolHandler::hasActiveTool(const QString &requestId) const
{
    return m_requestStates.contains(requestId) && m_requestStates[requestId].hasActiveTool();
}

QJsonObject ClaudeToolHandler::buildToolResultRequest(
    const QString &requestId, const QString &toolId, const QString &result)
{
    if (!m_requestStates.contains(requestId)) {
        LOG_MESSAGE(QString("No state found for request %1").arg(requestId));
        return QJsonObject();
    }

    const RequestState &state = m_requestStates[requestId];

    if (state.currentToolId != toolId) {
        LOG_MESSAGE(
            QString("Tool ID mismatch: expected %1, got %2").arg(state.currentToolId, toolId));
        return QJsonObject();
    }

    QJsonArray messages = state.originalRequest["messages"].toArray();

    // Add assistant message with text and tool_use
    QJsonObject assistantMessage;
    assistantMessage["role"] = "assistant";

    QJsonArray assistantContent;

    // Add text content if any
    const QString assistantText = state.assistantText.trimmed();
    if (!assistantText.isEmpty()) {
        QJsonObject textContent;
        textContent["type"] = "text";
        textContent["text"] = assistantText;
        assistantContent.append(textContent);
    }

    // Add tool use
    QJsonObject toolUse;
    toolUse["type"] = "tool_use";
    toolUse["id"] = toolId;
    toolUse["name"] = state.currentToolName;
    toolUse["input"] = state.currentToolInput;
    assistantContent.append(toolUse);

    assistantMessage["content"] = assistantContent;
    messages.append(assistantMessage);

    // Add tool result
    QJsonObject toolResultMessage;
    toolResultMessage["role"] = "user";

    QJsonArray toolResultContent;
    QJsonObject toolResult;
    toolResult["type"] = "tool_result";
    toolResult["tool_use_id"] = toolId;

    // Truncate result if too long
    QString truncatedResult = result.length() > 4000 ? result.left(4000) + "..." : result;
    toolResult["content"] = truncatedResult;

    toolResultContent.append(toolResult);
    toolResultMessage["content"] = toolResultContent;
    messages.append(toolResultMessage);

    // Build new request
    QJsonObject newRequest = state.originalRequest;
    newRequest["messages"] = messages;
    newRequest.remove("tools"); // Remove tools from continuation

    return newRequest;
}

void ClaudeToolHandler::executeToolAsync(
    const QString &requestId,
    const QString &toolName,
    const QString &toolId,
    const QJsonObject &input)
{
    if (!m_toolsFactory) {
        LOG_MESSAGE("ClaudeToolHandler: No tools factory available");
        return;
    }

    auto tool = m_toolsFactory->getToolByName(toolName);
    if (!tool) {
        LOG_MESSAGE(QString("ClaudeToolHandler: Tool not found: %1").arg(toolName));
        return;
    }

    QString result = tool->execute(input);
    LOG_MESSAGE(QString("ClaudeToolHandler: Executed tool %1").arg(toolName));

    // Build updated request and emit signal
    QJsonObject newRequest = buildToolResultRequest(requestId, toolId, result);
    if (!newRequest.isEmpty()) {
        emit toolResultReady(requestId, newRequest);
    }
}

} // namespace QodeAssist::Providers
