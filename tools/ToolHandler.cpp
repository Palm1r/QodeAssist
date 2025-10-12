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

#include "ToolHandler.hpp"

#include <QJsonDocument>
#include <QTimer>
#include <QtConcurrent>

#include "logger/Logger.hpp"

namespace QodeAssist::Tools {

ToolHandler::ToolHandler(QObject *parent)
    : QObject(parent)
{}

QFuture<QString> ToolHandler::executeToolAsync(
    const QString &requestId,
    const QString &toolId,
    LLMCore::BaseTool *tool,
    const QJsonObject &input)
{
    if (!tool) {
        return QtConcurrent::run([]() -> QString { throw std::runtime_error("Tool is null"); });
    }

    auto execution = std::make_unique<ToolExecution>();
    execution->requestId = requestId;
    execution->toolId = toolId;
    execution->toolName = tool->name();
    execution->watcher = new QFutureWatcher<QString>(this);

    connect(execution->watcher, &QFutureWatcher<QString>::finished, this, [this, toolId]() {
        onToolExecutionFinished(toolId);
    });

    LOG_MESSAGE(QString("Starting tool execution: %1 (ID: %2)").arg(tool->name(), toolId));

    auto future = tool->executeAsync(input);
    execution->watcher->setFuture(future);
    m_activeExecutions.insert(toolId, execution.release());

    return future;
}

void ToolHandler::cleanupRequest(const QString &requestId)
{
    auto it = m_activeExecutions.begin();
    while (it != m_activeExecutions.end()) {
        if (it.value()->requestId == requestId) {
            auto execution = it.value();
            LOG_MESSAGE(
                QString("Canceling tool %1 for request %2").arg(execution->toolName, requestId));

            if (execution->watcher) {
                execution->watcher->cancel();
                execution->watcher->deleteLater();
            }
            delete execution;
            it = m_activeExecutions.erase(it);
        } else {
            ++it;
        }
    }
}

void ToolHandler::onToolExecutionFinished(const QString &toolId)
{
    if (!m_activeExecutions.contains(toolId)) {
        return;
    }

    auto execution = m_activeExecutions.take(toolId);

    try {
        QString result = execution->watcher->result();
        LOG_MESSAGE(QString("Tool %1 completed").arg(execution->toolName));
        emit toolCompleted(execution->requestId, execution->toolId, result);
    } catch (const std::exception &e) {
        QString error = QString::fromStdString(e.what());
        LOG_MESSAGE(QString("Tool %1 failed: %2").arg(execution->toolName, error));
        emit toolFailed(execution->requestId, execution->toolId, error);
    }

    execution->watcher->deleteLater();
    delete execution;
}

} // namespace QodeAssist::Tools
