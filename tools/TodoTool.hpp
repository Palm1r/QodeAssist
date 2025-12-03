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

#pragma once

#include <llmcore/BaseTool.hpp>

#include <QHash>
#include <QMutex>
#include <QString>

namespace QodeAssist::Tools {

struct TodoItem
{
    int id;
    QString task;
    bool completed;
};

class TodoTool : public LLMCore::BaseTool
{
    Q_OBJECT

public:
    explicit TodoTool(QObject *parent = nullptr);

    QString name() const override;
    QString stringName() const override;
    QString description() const override;
    QJsonObject getDefinition(LLMCore::ToolSchemaFormat format) const override;
    LLMCore::ToolPermissions requiredPermissions() const override;

    QFuture<QString> executeAsync(const QJsonObject &input = QJsonObject()) override;

    void clearSession(const QString &sessionId);

private:
    QString addTodos(const QString &sessionId, const QStringList &tasks);
    QString completeTodos(const QString &sessionId, const QList<int> &todoIds);
    QString listTodos(const QString &sessionId) const;
    QString listTodosLocked(const QString &sessionId) const;
    QString listRemainingTodosLocked(const QString &sessionId) const;

    mutable QMutex m_mutex;
    QHash<QString, QHash<int, TodoItem>> m_sessionTodos;
    QHash<QString, int> m_sessionNextId;
};

} // namespace QodeAssist::Tools
