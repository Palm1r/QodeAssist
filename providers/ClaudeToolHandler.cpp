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

#include "logger/Logger.hpp"
#include <QDateTime>
#include <QJsonDocument>
#include <QTimer>

namespace QodeAssist::Providers {

ClaudeToolHandler::ClaudeToolHandler(QObject *parent)
    : QObject(parent)
{}

void ClaudeToolHandler::setToolsFactory(Tools::ToolsFactory *toolsFactory)
{
    m_toolsFactory = toolsFactory;
}

void ClaudeToolHandler::executeTool(
    const QString &requestId,
    const QString &toolId,
    const QString &toolName,
    const QJsonObject &input)
{
    if (!m_toolsFactory) {
        LOG_MESSAGE("No tools factory available");
        emit toolFailed(requestId, toolId, "No tools factory available");
        return;
    }

    auto tool = m_toolsFactory->getToolByName(toolName);
    if (!tool) {
        QString error = QString("Tool not found: %1").arg(toolName);
        LOG_MESSAGE(error);
        emit toolFailed(requestId, toolId, error);
        return;
    }

    // Сохраняем информацию о выполняемом инструменте
    ToolExecution execution;
    execution.requestId = requestId;
    execution.toolId = toolId;
    execution.toolName = toolName;
    execution.startTime = QDateTime::currentDateTime();

    m_activeTools[toolName] = execution;

    // Подключаем сигналы инструмента
    connect(
        tool,
        &LLMCore::ITool::toolCompleted,
        this,
        &ClaudeToolHandler::onToolCompleted,
        Qt::UniqueConnection);
    connect(
        tool,
        &LLMCore::ITool::toolFailed,
        this,
        &ClaudeToolHandler::onToolFailed,
        Qt::UniqueConnection);

    LOG_MESSAGE(QString("Executing tool: %1 with input: %2")
                    .arg(toolName)
                    .arg(QJsonDocument(input).toJson(QJsonDocument::Compact)));

    // Запускаем инструмент
    tool->execute(input);
}

void ClaudeToolHandler::cleanupRequest(const QString &requestId)
{
    // Удаляем все активные инструменты для данного запроса
    auto it = m_activeTools.begin();
    while (it != m_activeTools.end()) {
        if (it.value().requestId == requestId) {
            LOG_MESSAGE(
                QString("Cleaning up tool %1 for request %2").arg(it.value().toolName, requestId));
            it = m_activeTools.erase(it);
        } else {
            ++it;
        }
    }
}

void ClaudeToolHandler::onToolCompleted(const QString &toolName, const QString &result)
{
    if (!m_activeTools.contains(toolName)) {
        LOG_MESSAGE(QString("Received completion for unknown tool: %1").arg(toolName));
        return;
    }

    ToolExecution execution = m_activeTools.take(toolName);

    qint64 executionTime = execution.startTime.msecsTo(QDateTime::currentDateTime());
    LOG_MESSAGE(QString("Tool %1 completed in %2ms").arg(toolName).arg(executionTime));

    emit toolCompleted(execution.requestId, execution.toolId, result);
}

void ClaudeToolHandler::onToolFailed(const QString &toolName, const QString &error)
{
    if (!m_activeTools.contains(toolName)) {
        LOG_MESSAGE(QString("Received failure for unknown tool: %1").arg(toolName));
        return;
    }

    ToolExecution execution = m_activeTools.take(toolName);

    qint64 executionTime = execution.startTime.msecsTo(QDateTime::currentDateTime());
    LOG_MESSAGE(
        QString("Tool %1 failed after %2ms: %3").arg(toolName).arg(executionTime).arg(error));

    emit toolFailed(execution.requestId, execution.toolId, error);
}

} // namespace QodeAssist::Providers
