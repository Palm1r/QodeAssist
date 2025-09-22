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

#pragma once

#include <QFutureWatcher>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <llmcore/BaseTool.hpp>

namespace QodeAssist::Tools {

class ToolHandler : public QObject
{
    Q_OBJECT

public:
    explicit ToolHandler(QObject *parent = nullptr);

    QFuture<QString> executeToolAsync(
        const QString &requestId,
        const QString &toolId,
        LLMCore::BaseTool *tool,
        const QJsonObject &input);

    void cleanupRequest(const QString &requestId);

signals:
    void toolCompleted(const QString &requestId, const QString &toolId, const QString &result);
    void toolFailed(const QString &requestId, const QString &toolId, const QString &error);

private:
    struct ToolExecution
    {
        QString requestId;
        QString toolId;
        QString toolName;
        QFutureWatcher<QString> *watcher;
    };

    QHash<QString, ToolExecution *> m_activeExecutions; // toolId -> execution

    void onToolExecutionFinished(const QString &toolId);
};

} // namespace QodeAssist::Tools
