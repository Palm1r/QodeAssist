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

#include "GetIssuesListTool.hpp"

#include <Version.hpp>
#include <logger/Logger.hpp>
#include <projectexplorer/taskhub.h>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutexLocker>
#include <QtConcurrent>

namespace QodeAssist::Tools {

IssuesTracker &IssuesTracker::instance()
{
    static IssuesTracker tracker;
    return tracker;
}

IssuesTracker::IssuesTracker(QObject *parent)
    : QObject(parent)
{

    auto &hub = ProjectExplorer::taskHub();

    connect(&hub, &ProjectExplorer::TaskHub::taskAdded, this, &IssuesTracker::onTaskAdded);
    connect(&hub, &ProjectExplorer::TaskHub::taskRemoved, this, &IssuesTracker::onTaskRemoved);
    connect(&hub, &ProjectExplorer::TaskHub::tasksCleared, this, &IssuesTracker::onTasksCleared);

}

QList<ProjectExplorer::Task> IssuesTracker::getTasks() const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks;
}

void IssuesTracker::onTaskAdded(const ProjectExplorer::Task &task)
{
    QMutexLocker locker(&m_mutex);
    m_tasks.append(task);

    QString typeStr;
#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(18, 0, 0)
    auto taskType = task.type();
    auto taskFile = task.file();
    auto taskLine = task.line();
#else
    auto taskType = task.type;
    auto taskFile = task.file;
    auto taskLine = task.line;
#endif
    switch (taskType) {
    case ProjectExplorer::Task::Error:
        typeStr = "ERROR";
        break;
    case ProjectExplorer::Task::Warning:
        typeStr = "WARNING";
        break;
    default:
        typeStr = "INFO";
        break;
    }

}

void IssuesTracker::onTaskRemoved(const ProjectExplorer::Task &task)
{
    QMutexLocker locker(&m_mutex);
    m_tasks.removeOne(task);

}

void IssuesTracker::onTasksCleared(Utils::Id categoryId)
{
    QMutexLocker locker(&m_mutex);

    if (categoryId.isValid()) {
        int beforeCount = m_tasks.size();
        m_tasks.erase(
            std::remove_if(
                m_tasks.begin(),
                m_tasks.end(),
                [categoryId](const ProjectExplorer::Task &task) {
#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(18, 0, 0)
                    return task.category() == categoryId;
#else
                    return task.category == categoryId;
#endif
                }),
            m_tasks.end());
        int removedCount = beforeCount - m_tasks.size();

    } else {
        int clearedCount = m_tasks.size();
        m_tasks.clear();
    }
}

GetIssuesListTool::GetIssuesListTool(QObject *parent)
    : BaseTool(parent)
{
    IssuesTracker::instance();
}

QString GetIssuesListTool::name() const
{
    return "get_issues_list";
}

QString GetIssuesListTool::stringName() const
{
    return "Getting issues list from Qt Creator";
}

QString GetIssuesListTool::description() const
{
    return "Get compilation errors, warnings, and diagnostics from Qt Creator's Issues panel. "
           "Returns issue descriptions with file paths and line numbers. "
           "Optional severity filter: 'error', 'warning', or 'all' (default).";
}

QJsonObject GetIssuesListTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject definition;
    definition["type"] = "object";

    QJsonObject properties;
    properties["severity"] = QJsonObject{
        {"type", "string"},
        {"description", "Filter by severity: 'error', 'warning', or 'all'"},
        {"enum", QJsonArray{"error", "warning", "all"}}};

    definition["properties"] = properties;
    definition["required"] = QJsonArray();

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

LLMCore::ToolPermissions GetIssuesListTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::FileSystemRead;
}

QFuture<QString> GetIssuesListTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([input]() -> QString {

        QString severityFilter = input.value("severity").toString("all");

        const auto tasks = IssuesTracker::instance().getTasks();

        if (tasks.isEmpty()) {
            return "No issues found in Qt Creator Issues panel.";
        }

        QStringList results;
        results.append(QString("Total issues in panel: %1\n").arg(tasks.size()));

        int errorCount = 0;
        int warningCount = 0;
        int processedCount = 0;

        for (const ProjectExplorer::Task &task : tasks) {

#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(18, 0, 0)
            auto taskType = task.type();
            auto taskFile = task.file();
            auto taskLine = task.line();
            auto taskColumn = task.column();
            auto taskCategory = task.category();
#else
            auto taskType = task.type;
            auto taskFile = task.file;
            auto taskLine = task.line;
            auto taskColumn = task.column;
            auto taskCategory = task.category;
#endif
            if (severityFilter == "error" && taskType != ProjectExplorer::Task::Error)
                continue;
            if (severityFilter == "warning" && taskType != ProjectExplorer::Task::Warning)
                continue;

            QString typeStr;
            switch (taskType) {
            case ProjectExplorer::Task::Error:
                typeStr = "ERROR";
                errorCount++;
                break;
            case ProjectExplorer::Task::Warning:
                typeStr = "WARNING";
                warningCount++;
                break;
            default:
                typeStr = "INFO";
                break;
            }

            QString issueText = QString("[%1] %2").arg(typeStr, task.description());

            if (!taskFile.isEmpty()) {
                issueText += QString("\n  File: %1").arg(taskFile.toUrlishString());
                if (taskLine > 0) {
                    issueText += QString(":%1").arg(taskLine);
                    if (taskColumn > 0) {
                        issueText += QString(":%1").arg(taskColumn);
                    }
                }
            }

            if (!taskCategory.toString().isEmpty()) {
                issueText += QString("\n  Category: %1").arg(taskCategory.toString());
            }

            results.append(issueText);
            processedCount++;
        }

        QString summary = QString("\nSummary: %1 errors, %2 warnings (processed %3 tasks)")
                              .arg(errorCount)
                              .arg(warningCount)
                              .arg(processedCount);
        results.prepend(summary);

        return results.join("\n\n");
    });
}

} // namespace QodeAssist::Tools
