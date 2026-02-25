/*
 * Copyright (C) 2025-2026 Petr Mironychev
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
#include <Logger.hpp>
#include <QTimer>

namespace {
constexpr int kToolExecutionDelayMs = 300;
}

namespace QodeAssist::LLMCore {

ToolsManager::ToolsManager(QObject *parent)
    : QObject(parent)
    , m_toolHandler(new ToolHandler(this))
    , m_allowedPermissions(ToolPermission::FileSystemRead | ToolPermission::FileSystemWrite
                           | ToolPermission::NetworkAccess)
{
    initConnections();
}

void ToolsManager::initConnections()
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

void ToolsManager::addTool(BaseTool *tool)
{
    if (!tool) {
        LOG_MESSAGE("ToolsManager: Attempted to add null tool");
        return;
    }

    const QString toolName = tool->name();
    if (m_tools.contains(toolName)) {
        LOG_MESSAGE(QString("ToolsManager: Tool '%1' already registered, replacing").arg(toolName));
    }

    tool->setParent(this);
    m_tools.insert(toolName, tool);
    LOG_MESSAGE(QString("ToolsManager: Added tool '%1'").arg(toolName));
}

void ToolsManager::removeTool(const QString &name)
{
    if (auto *t = m_tools.take(name)) {
        t->deleteLater();
        LOG_MESSAGE(QString("ToolsManager: Removed tool '%1'").arg(name));
    }
}

BaseTool *ToolsManager::tool(const QString &name) const
{
    return m_tools.value(name, nullptr);
}

QList<BaseTool *> ToolsManager::registeredTools() const
{
    return m_tools.values();
}

void ToolsManager::setAllowedPermissions(ToolPermissions permissions)
{
    m_allowedPermissions = permissions;
}

ToolPermissions ToolsManager::allowedPermissions() const
{
    return m_allowedPermissions;
}

QString ToolsManager::stringName(const QString &toolName) const
{
    if (auto *t = m_tools.value(toolName)) {
        return t->stringName();
    }
    return QStringLiteral("Unknown tool");
}

void ToolsManager::clearSession(const QString &sessionId)
{
    for (auto *t : m_tools) {
        t->clearSession(sessionId);
    }
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

    PendingTool pendingTool = queue.queue.takeFirst();
    queue.isExecuting = true;

    LOG_MESSAGE(QString("ToolsManager: Executing tool %1 (ID: %2) for request %3 (%4 remaining)")
                    .arg(pendingTool.name, pendingTool.id, requestId)
                    .arg(queue.queue.size()));

    BaseTool *toolInstance = m_tools.value(pendingTool.name);
    if (!toolInstance) {
        LOG_MESSAGE(QString("ToolsManager: Tool not found: %1").arg(pendingTool.name));
        pendingTool.result = QString("Error: Tool not found: %1").arg(pendingTool.name);
        pendingTool.complete = true;
        queue.completed[pendingTool.id] = pendingTool;
        executeNextTool(requestId);
        return;
    }

    queue.completed[pendingTool.id] = pendingTool;

    m_toolHandler->executeToolAsync(requestId, pendingTool.id, toolInstance, pendingTool.input);
    LOG_MESSAGE(QString("ToolsManager: Started async execution of %1").arg(pendingTool.name));
}

QJsonArray ToolsManager::getToolsDefinitions(
    ToolSchemaFormat format, RunToolsFilter filter) const
{
    return buildToolsDefinitions(format, filter);
}

QJsonArray ToolsManager::buildToolsDefinitions(
    ToolSchemaFormat format, RunToolsFilter filter) const
{
    QJsonArray toolsArray;

    for (auto it = m_tools.constBegin(); it != m_tools.constEnd(); ++it) {
        BaseTool *t = it.value();
        if (!t || !t->isEnabled()) {
            continue;
        }

        const auto requiredPerms = t->requiredPermissions();

        // Apply RunToolsFilter
        if (filter != RunToolsFilter::ALL) {
            bool matchesFilter = false;

            switch (filter) {
            case RunToolsFilter::OnlyRead:
                if (requiredPerms == ToolPermission::None
                    || requiredPerms.testFlag(ToolPermission::FileSystemRead)) {
                    matchesFilter = true;
                }
                break;
            case RunToolsFilter::OnlyWrite:
                if (requiredPerms.testFlag(ToolPermission::FileSystemWrite)) {
                    matchesFilter = true;
                }
                break;
            case RunToolsFilter::OnlyNetworking:
                if (requiredPerms.testFlag(ToolPermission::NetworkAccess)) {
                    matchesFilter = true;
                }
                break;
            case RunToolsFilter::ALL:
                matchesFilter = true;
                break;
            }

            if (!matchesFilter) {
                continue;
            }
        }

        // Apply permission filter
        bool hasPermission = true;

        if (requiredPerms.testFlag(ToolPermission::FileSystemRead)
            && !m_allowedPermissions.testFlag(ToolPermission::FileSystemRead)) {
            hasPermission = false;
        }
        if (requiredPerms.testFlag(ToolPermission::FileSystemWrite)
            && !m_allowedPermissions.testFlag(ToolPermission::FileSystemWrite)) {
            hasPermission = false;
        }
        if (requiredPerms.testFlag(ToolPermission::NetworkAccess)
            && !m_allowedPermissions.testFlag(ToolPermission::NetworkAccess)) {
            hasPermission = false;
        }

        if (hasPermission) {
            toolsArray.append(t->getDefinition(format));
        }
    }

    return toolsArray;
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

    PendingTool &pendingTool = queue.completed[toolId];
    pendingTool.result = success ? result : QString("Error: %1").arg(result);
    pendingTool.complete = true;

    LOG_MESSAGE(QString("ToolsManager: Tool %1 %2 for request %3")
                    .arg(toolId)
                    .arg(success ? QString("completed") : QString("failed"))
                    .arg(requestId));

    if (kToolExecutionDelayMs > 0 && !queue.queue.isEmpty()) {
        QTimer::singleShot(kToolExecutionDelayMs, this, [this, requestId]() {
            executeNextTool(requestId);
        });
    } else {
        executeNextTool(requestId);
    }
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

void ToolsManager::setCurrentSessionId(const QString &sessionId)
{
    m_currentSessionId = sessionId;
}

} // namespace QodeAssist::LLMCore
