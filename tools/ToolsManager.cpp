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
    LOG_MESSAGE(QString("ToolsManager: Executing tool %1 (ID: %2) for request %3")
                    .arg(toolName, toolId, requestId));

    if (!m_pendingTools.contains(requestId)) {
        m_pendingTools[requestId] = QHash<QString, PendingTool>();
    }

    if (m_pendingTools[requestId].contains(toolId)) {
        LOG_MESSAGE(QString("Tool %1 already in progress for request %2").arg(toolId, requestId));
        return;
    }

    auto tool = m_toolsFactory->getToolByName(toolName);
    if (!tool) {
        LOG_MESSAGE(QString("ToolsManager: Tool not found: %1").arg(toolName));
        return;
    }

    PendingTool pendingTool{toolId, toolName, input, "", false};
    m_pendingTools[requestId][toolId] = pendingTool;

    m_toolHandler->executeToolAsync(requestId, toolId, tool, input);
    LOG_MESSAGE(QString("ToolsManager: Started async execution of %1").arg(toolName));
}

QJsonArray ToolsManager::getToolsDefinitions(LLMCore::ToolSchemaFormat format) const
{
    if (!m_toolsFactory) {
        return QJsonArray();
    }

    return m_toolsFactory->getToolsDefinitions(format);
}

void ToolsManager::cleanupRequest(const QString &requestId)
{
    if (m_pendingTools.contains(requestId)) {
        LOG_MESSAGE(QString("ToolsManager: Canceling pending tools for request %1").arg(requestId));
        m_toolHandler->cleanupRequest(requestId);
        m_pendingTools.remove(requestId);
    }

    LOG_MESSAGE(QString("ToolsManager: Cleaned up request %1").arg(requestId));
}

void ToolsManager::onToolFinished(
    const QString &requestId, const QString &toolId, const QString &result, bool success)
{
    if (!m_pendingTools.contains(requestId) || !m_pendingTools[requestId].contains(toolId)) {
        LOG_MESSAGE(QString("ToolsManager: Tool result for unknown tool %1 in request %2")
                        .arg(toolId, requestId));
        return;
    }

    PendingTool &tool = m_pendingTools[requestId][toolId];
    tool.result = success ? result : QString("Error: %1").arg(result);
    tool.complete = true;

    LOG_MESSAGE(QString("ToolsManager: Tool %1 %2 for request %3")
                    .arg(toolId)
                    .arg(success ? QString("completed") : QString("failed"))
                    .arg(requestId));

    if (isExecutionComplete(requestId)) {
        QHash<QString, QString> results = getToolResults(requestId);
        LOG_MESSAGE(QString("ToolsManager: All tools complete for request %1, emitting results")
                        .arg(requestId));
        emit toolExecutionComplete(requestId, results);
    } else {
        LOG_MESSAGE(QString("ToolsManager: Tools still pending for request %1").arg(requestId));
    }
}

ToolsFactory *ToolsManager::toolsFactory() const
{
    return m_toolsFactory;
}

bool ToolsManager::isExecutionComplete(const QString &requestId) const
{
    if (!m_pendingTools.contains(requestId)) {
        return true;
    }

    const auto &tools = m_pendingTools[requestId];
    for (auto it = tools.begin(); it != tools.end(); ++it) {
        if (!it.value().complete) {
            return false;
        }
    }

    return true;
}

QHash<QString, QString> ToolsManager::getToolResults(const QString &requestId) const
{
    QHash<QString, QString> results;

    if (m_pendingTools.contains(requestId)) {
        const auto &tools = m_pendingTools[requestId];
        for (auto it = tools.begin(); it != tools.end(); ++it) {
            if (it.value().complete) {
                results[it.key()] = it.value().result;
            }
        }
    }

    return results;
}

} // namespace QodeAssist::Tools
