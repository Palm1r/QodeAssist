// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LLMQore/BaseTool.hpp>

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

class TodoTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    explicit TodoTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;

    void setCurrentSessionId(const QString &sessionId);
    void clearSession(const QString &sessionId);

private:
    QString addTodos(const QString &sessionId, const QStringList &tasks);
    QString completeTodos(const QString &sessionId, const QList<int> &todoIds);
    QString listTodos(const QString &sessionId) const;
    QString listTodosLocked(const QString &sessionId) const;
    QString listRemainingTodosLocked(const QString &sessionId) const;

    mutable QMutex m_mutex;
    QString m_currentSessionId;
    QHash<QString, QHash<int, TodoItem>> m_sessionTodos;
    QHash<QString, int> m_sessionNextId;
};

} // namespace QodeAssist::Tools
