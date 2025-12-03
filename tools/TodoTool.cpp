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

#include "TodoTool.hpp"
#include "ToolExceptions.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QMutexLocker>
#include <QtConcurrent>

namespace QodeAssist::Tools {

TodoTool::TodoTool(QObject *parent)
    : BaseTool(parent)
{}

QString TodoTool::name() const
{
    return "todo_tool";
}

QString TodoTool::stringName() const
{
    return "Managing TODO list for task tracking";
}

QString TodoTool::description() const
{
    return "Track and organize multi-step tasks during complex operations that require multiple "
           "sequential steps. "
           "**Use when planning 3+ step workflows.** "
           "Operations: 'add' - provide array of task descriptions to create full plan at once, "
           "'complete' - provide array of task IDs to mark finished steps, 'list' - review "
           "progress. "
           "Helpful for: large refactorings, feature implementations, debugging workflows. "
           "The list persists throughout the conversation.";
}

QJsonObject TodoTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject definition;
    definition["type"] = "object";

    QJsonObject properties;

    QJsonObject operationProp;
    operationProp["type"] = "string";
    operationProp["description"] = "Operation: 'add' (create tasks), 'complete' (mark tasks as "
                                    "done), 'list' (show all tasks)";
    QJsonArray operationEnum;
    operationEnum.append("add");
    operationEnum.append("complete");
    operationEnum.append("list");
    operationProp["enum"] = operationEnum;
    properties["operation"] = operationProp;

    QJsonObject tasksProp;
    tasksProp["type"] = "array";
    QJsonObject tasksItems;
    tasksItems["type"] = "string";
    tasksProp["items"] = tasksItems;
    tasksProp["description"]
        = "Array of task descriptions to create (required for 'add' operation). "
          "Create all subtasks at once, e.g.: ['Step 1: ...', 'Step 2: ...', 'Step 3: ...']";
    properties["tasks"] = tasksProp;

    QJsonObject todoIdsProp;
    todoIdsProp["type"] = "array";
    QJsonObject todoIdsItems;
    todoIdsItems["type"] = "integer";
    todoIdsProp["items"] = todoIdsItems;
    todoIdsProp["description"]
        = "Array of todo item IDs to mark as completed (required for 'complete' operation). "
          "Example: [1, 2, 5] to complete tasks #1, #2, and #5";
    properties["todo_ids"] = todoIdsProp;

    definition["properties"] = properties;

    QJsonArray required;
    required.append("operation");
    definition["required"] = required;

    switch (format) {
    case LLMCore::ToolSchemaFormat::OpenAI:
        return customizeForOpenAI(definition);
    case LLMCore::ToolSchemaFormat::Claude:
        return customizeForClaude(definition);
    case LLMCore::ToolSchemaFormat::Ollama:
        return customizeForOllama(definition);
    case LLMCore::ToolSchemaFormat::Google:
        return customizeForGoogle(definition);
    }

    return definition;
}

LLMCore::ToolPermissions TodoTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::None;
}

QFuture<QString> TodoTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString sessionId = input.value("session_id").toString();
        if (sessionId.isEmpty()) {
            sessionId = "current";
        }

        const QString operation = input.value("operation").toString();

        if (operation == "add") {
            if (!input.contains("tasks") || !input.value("tasks").isArray()) {
                throw ToolRuntimeError(
                    tr("Error: 'tasks' parameter (array) is required for 'add' operation. "
                       "Example: {\"operation\": \"add\", \"tasks\": [\"Task 1\", \"Task 2\"]}"));
            }

            const QJsonArray tasksArray = input.value("tasks").toArray();
            if (tasksArray.isEmpty()) {
                throw ToolRuntimeError(
                    tr("Error: 'tasks' array cannot be empty. Provide at least one task."));
            }

            QStringList tasks;
            for (const QJsonValue &taskValue : tasksArray) {
                QString task = taskValue.toString().trimmed();
                if (!task.isEmpty()) {
                    tasks.append(task);
                }
            }

            if (tasks.isEmpty()) {
                throw ToolRuntimeError(
                    tr("Error: All tasks in 'tasks' array are empty strings."));
            }

            return addTodos(sessionId, tasks);

        } else if (operation == "complete") {
            if (!input.contains("todo_ids") || !input.value("todo_ids").isArray()) {
                throw ToolRuntimeError(
                    tr("Error: 'todo_ids' parameter (array) is required for 'complete' operation. "
                       "Example: {\"operation\": \"complete\", \"todo_ids\": [1, 2, 3]}"));
            }

            const QJsonArray idsArray = input.value("todo_ids").toArray();
            if (idsArray.isEmpty()) {
                throw ToolRuntimeError(
                    tr("Error: 'todo_ids' array cannot be empty. Provide at least one ID."));
            }

            QList<int> ids;
            for (const QJsonValue &idValue : idsArray) {
                int id = idValue.toInt(-1);
                if (id > 0) {
                    ids.append(id);
                }
            }

            if (ids.isEmpty()) {
                throw ToolRuntimeError(
                    tr("Error: All IDs in 'todo_ids' array are invalid. IDs must be positive "
                       "integers."));
            }

            return completeTodos(sessionId, ids);

        } else if (operation == "list") {
            return listTodos(sessionId);

        } else {
            throw ToolRuntimeError(
                tr("Error: Unknown operation '%1'. Valid operations: 'add', 'complete', 'list'")
                    .arg(operation));
        }
    });
}

void TodoTool::clearSession(const QString &sessionId)
{
    QMutexLocker locker(&m_mutex);
    m_sessionTodos.remove(sessionId);
    m_sessionNextId.remove(sessionId);
}

QString TodoTool::addTodos(const QString &sessionId, const QStringList &tasks)
{
    QMutexLocker locker(&m_mutex);

    if (!m_sessionTodos.contains(sessionId)) {
        m_sessionTodos[sessionId] = QHash<int, TodoItem>();
        m_sessionNextId[sessionId] = 1;
    }

    for (const QString &task : tasks) {
        const int newId = m_sessionNextId[sessionId]++;
        m_sessionTodos[sessionId][newId] = {newId, task, false};
    }

    const QString summary = (tasks.size() == 1) ? tr("✓ Added 1 new task")
                                                 : tr("✓ Added %1 new tasks").arg(tasks.size());

    return QString("%1\n\n%2").arg(summary, listTodosLocked(sessionId));
}

QString TodoTool::completeTodos(const QString &sessionId, const QList<int> &todoIds)
{
    QMutexLocker locker(&m_mutex);

    if (!m_sessionTodos.contains(sessionId)) {
        throw ToolRuntimeError(tr("Error: No todos found in this session"));
    }

    auto &todos = m_sessionTodos[sessionId];
    int completedCount = 0;
    int alreadyCompletedCount = 0;
    QStringList notFound;

    for (const int todoId : todoIds) {
        if (!todos.contains(todoId)) {
            notFound.append(QString("#%1").arg(todoId));
            continue;
        }

        TodoItem &item = todos[todoId];
        if (item.completed) {
            alreadyCompletedCount++;
        } else {
            item.completed = true;
            completedCount++;
        }
    }

    QStringList messages;
    if (completedCount > 0) {
        messages.append((completedCount == 1) ? tr("✓ Marked 1 task as completed")
                                               : tr("✓ Marked %1 tasks as completed")
                                                     .arg(completedCount));
    }

    if (alreadyCompletedCount > 0) {
        messages.append(tr("⚠ %1 already completed").arg(alreadyCompletedCount));
    }

    if (!notFound.isEmpty()) {
        messages.append(tr("❌ Not found: %1").arg(notFound.join(", ")));
    }

    const QString summary = messages.join(", ");
    return QString("%1\n\n%2").arg(summary, listRemainingTodosLocked(sessionId));
}

QString TodoTool::listTodos(const QString &sessionId) const
{
    QMutexLocker locker(&m_mutex);
    return listTodosLocked(sessionId);
}

QString TodoTool::listTodosLocked(const QString &sessionId) const
{
    const auto it = m_sessionTodos.constFind(sessionId);
    if (it == m_sessionTodos.constEnd() || it->isEmpty()) {
        return tr("📋 TODO List: (empty)");
    }

    const auto &todos = *it;
    QList<int> ids = todos.keys();
    std::sort(ids.begin(), ids.end());

    QStringList lines;
    lines.reserve(ids.size() + 4);
    lines.append(tr("📋 TODO List:"));
    lines.append("");

    int completedCount = 0;
    for (const int id : ids) {
        const TodoItem &item = todos[id];
        const QString checkbox = item.completed ? "[x]" : "[ ]";
        const QString strikethrough = item.completed ? QString("~~") : QString("");

        lines.append(QString("%1 **#%2** %3%4%5")
                         .arg(checkbox)
                         .arg(id)
                         .arg(strikethrough, item.task, strikethrough));

        if (item.completed) {
            completedCount++;
        }
    }

    lines.append("");
    const int totalCount = ids.size();
    const int percentage = totalCount > 0 ? (completedCount * 100) / totalCount : 0;
    lines.append(
        tr("Progress: %1/%2 completed (%3%)").arg(completedCount).arg(totalCount).arg(percentage));

    return lines.join("\n");
}

QString TodoTool::listRemainingTodosLocked(const QString &sessionId) const
{
    const auto it = m_sessionTodos.constFind(sessionId);
    if (it == m_sessionTodos.constEnd() || it->isEmpty()) {
        return tr("📋 All tasks completed! 🎉");
    }

    const auto &todos = *it;
    QList<int> ids = todos.keys();
    std::sort(ids.begin(), ids.end());

    int completedCount = 0;
    QStringList remainingLines;

    for (const int id : ids) {
        const TodoItem &item = todos[id];
        if (item.completed) {
            completedCount++;
        } else {
            remainingLines.append(QString("[ ] **#%1** %2").arg(id).arg(item.task));
        }
    }

    if (remainingLines.isEmpty()) {
        return tr("📋 All tasks completed! 🎉");
    }

    QStringList lines;
    lines.reserve(remainingLines.size() + 4);
    lines.append(tr("📋 Remaining tasks:"));
    lines.append("");
    lines.append(remainingLines);
    lines.append("");

    const int totalCount = ids.size();
    const int percentage = totalCount > 0 ? (completedCount * 100) / totalCount : 0;
    lines.append(
        tr("Progress: %1/%2 completed (%3%)").arg(completedCount).arg(totalCount).arg(percentage));

    return lines.join("\n");
}

} // namespace QodeAssist::Tools
