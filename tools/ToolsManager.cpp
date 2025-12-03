/* 
 * Copyright (C) 2025 Petr Mironychev
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

#include "ToolsManager.hpp"
#include "TodoTool.hpp"
#include "logger/Logger.hpp"

namespace QodeAssist::Tools {

ToolsManager::ToolsManager(QObject *parent)
    : QObject(parent)
    , m_toolsFactory(new ToolsFactory(this))
    , m_toolHandler(new ToolHandler(this))
{
    connect(
        m_toolHandler,
        &ToolHandler::toolCompleted,
        this,
        [this](const QString &requestId, const QString &toolId, const QString &result) {
            onToolFinished(requestId, toolId, result, true);
        });

    connect(
        m_toolHandler,
        &ToolHandler::toolFailed,
        this,
        [this](const QString &requestId, const QString &toolId, const QString &error) {
            onToolFinished(requestId, toolId, error, false);
        });
}

void ToolsManager::executeToolCall(
    const QString &requestId,
    const QString &toolId,
    const QString &toolName,
    const QJsonObject &input)
{
    LOG_MESSAGE(QString("ToolsManager: Queueing tool %1 (ID: %2) for request %3")
                    .arg(toolName, toolId, requestId));

    if (!m_toolQueues.contains(requestId)) {
        m_toolQueues[requestId] = ToolQueue();
    }

    auto &queue = m_toolQueues[requestId];

    for (const auto &tool : queue.queue) {
        if (tool.id == toolId) {
            LOG_MESSAGE(QString("Tool %1 already in queue for request %2").arg(toolId, requestId));
            return;
        }
    }

    if (queue.completed.contains(toolId)) {
        LOG_MESSAGE(
            QString("Tool %1 already completed for request %2").arg(toolId, requestId));
        return;
    }

    QJsonObject modifiedInput = input;
    modifiedInput["_request_id"] = requestId;
    
    if (!m_currentSessionId.isEmpty()) {
        modifiedInput["session_id"] = m_currentSessionId;
    }

    PendingTool pendingTool{toolId, toolName, modifiedInput, "", false};
    queue.queue.append(pendingTool);

    LOG_MESSAGE(QString("ToolsManager: Tool %1 added to queue (position %2)")
                    .arg(toolName)
                    .arg(queue.queue.size()));

    if (!queue.isExecuting) {
        executeNextTool(requestId);
    }
}

void ToolsManager::executeNextTool(const QString &requestId)
{
    if (!m_toolQueues.contains(requestId)) {
        return;
    }

    auto &queue = m_toolQueues[requestId];

    if (queue.queue.isEmpty()) {
        LOG_MESSAGE(QString("ToolsManager: All tools complete for request %1, emitting results")
                        .arg(requestId));
        QHash<QString, QString> results = getToolResults(requestId);
        emit toolExecutionComplete(requestId, results);
        queue.isExecuting = false;
        return;
    }

    PendingTool tool = queue.queue.takeFirst();
    queue.isExecuting = true;

    LOG_MESSAGE(QString("ToolsManager: Executing tool %1 (ID: %2) for request %3 (%4 remaining)")
                    .arg(tool.name, tool.id, requestId)
                    .arg(queue.queue.size()));

    auto toolInstance = m_toolsFactory->getToolByName(tool.name);
    if (!toolInstance) {
        LOG_MESSAGE(QString("ToolsManager: Tool not found: %1").arg(tool.name));
        tool.result = QString("Error: Tool not found: %1").arg(tool.name);
        tool.complete = true;
        queue.completed[tool.id] = tool;
        executeNextTool(requestId);
        return;
    }

    queue.completed[tool.id] = tool;

    m_toolHandler->executeToolAsync(requestId, tool.id, toolInstance, tool.input);
    LOG_MESSAGE(QString("ToolsManager: Started async execution of %1").arg(tool.name));
}

QJsonArray ToolsManager::getToolsDefinitions(
    LLMCore::ToolSchemaFormat format, LLMCore::RunToolsFilter filter) const
{
    if (!m_toolsFactory) {
        return QJsonArray();
    }

    return m_toolsFactory->getToolsDefinitions(format, filter);
}

void ToolsManager::cleanupRequest(const QString &requestId)
{
    if (m_toolQueues.contains(requestId)) {
        m_toolHandler->cleanupRequest(requestId);
        m_toolQueues.remove(requestId);
    }
}

void ToolsManager::onToolFinished(
    const QString &requestId, const QString &toolId, const QString &result, bool success)
{
    if (!m_toolQueues.contains(requestId)) {
        return;
    }

    auto &queue = m_toolQueues[requestId];

    if (!queue.completed.contains(toolId)) {
        return;
    }

    PendingTool &tool = queue.completed[toolId];
    tool.result = success ? result : QString("Error: %1").arg(result);
    tool.complete = true;

    LOG_MESSAGE(QString("ToolsManager: Tool %1 %2 for request %3")
                    .arg(toolId)
                    .arg(success ? QString("completed") : QString("failed"))
                    .arg(requestId));

    executeNextTool(requestId);
}

ToolsFactory *ToolsManager::toolsFactory() const
{
    return m_toolsFactory;
}

QHash<QString, QString> ToolsManager::getToolResults(const QString &requestId) const
{
    QHash<QString, QString> results;

    if (m_toolQueues.contains(requestId)) {
        const auto &queue = m_toolQueues[requestId];
        for (auto it = queue.completed.begin(); it != queue.completed.end(); ++it) {
            if (it.value().complete) {
                results[it.key()] = it.value().result;
            }
        }
    }

    return results;
}

void ToolsManager::clearTodoSession(const QString &sessionId)
{
    auto *todoTool = qobject_cast<TodoTool *>(m_toolsFactory->getToolByName("todo_tool"));
    if (todoTool) {
        todoTool->clearSession(sessionId);
    }
}

void ToolsManager::setCurrentSessionId(const QString &sessionId)
{
    m_currentSessionId = sessionId;
}

} // namespace QodeAssist::Tools
