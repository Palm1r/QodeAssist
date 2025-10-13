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
    LOG_MESSAGE("IssuesTracker: Initializing tracker");

    auto &hub = ProjectExplorer::taskHub();

    connect(&hub, &ProjectExplorer::TaskHub::taskAdded, this, &IssuesTracker::onTaskAdded);
    connect(&hub, &ProjectExplorer::TaskHub::taskRemoved, this, &IssuesTracker::onTaskRemoved);
    connect(&hub, &ProjectExplorer::TaskHub::tasksCleared, this, &IssuesTracker::onTasksCleared);

    LOG_MESSAGE("IssuesTracker: Connected to TaskHub signals");
}

QList<ProjectExplorer::Task> IssuesTracker::getTasks() const
{
    QMutexLocker locker(&m_mutex);
    LOG_MESSAGE(QString("IssuesTracker: getTasks() called, current count: %1").arg(m_tasks.size()));
    return m_tasks;
}

void IssuesTracker::onTaskAdded(const ProjectExplorer::Task &task)
{
    QMutexLocker locker(&m_mutex);
    m_tasks.append(task);

    QString typeStr;
    switch (task.type) {
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

    LOG_MESSAGE(QString("IssuesTracker: Task added [%1] %2 at %3:%4 (total: %5)")
                    .arg(typeStr)
                    .arg(task.description())
                    .arg(task.file.toUrlishString())
                    .arg(task.line)
                    .arg(m_tasks.size()));
}

void IssuesTracker::onTaskRemoved(const ProjectExplorer::Task &task)
{
    QMutexLocker locker(&m_mutex);
    m_tasks.removeOne(task);

    LOG_MESSAGE(QString("IssuesTracker: Task removed: %1 (total: %2)")
                    .arg(task.description())
                    .arg(m_tasks.size()));
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
                    return task.category == categoryId;
                }),
            m_tasks.end());
        int removedCount = beforeCount - m_tasks.size();

        LOG_MESSAGE(
            QString("IssuesTracker: Tasks cleared for category %1, removed %2 tasks (total: %3)")
                .arg(categoryId.toString())
                .arg(removedCount)
                .arg(m_tasks.size()));
    } else {
        int clearedCount = m_tasks.size();
        m_tasks.clear();
        LOG_MESSAGE(QString("IssuesTracker: All tasks cleared, removed %1 tasks").arg(clearedCount));
    }
}

GetIssuesListTool::GetIssuesListTool(QObject *parent)
    : BaseTool(parent)
{
    LOG_MESSAGE("GetIssuesListTool: Initializing tool");
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
    return "Get list of errors, warnings and other issues from Qt Creator's Issues panel. "
           "Returns information about compilation errors, static analysis warnings, and other "
           "diagnostic messages.";
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
        LOG_MESSAGE("GetIssuesListTool: Starting execution");

        QString severityFilter = input.value("severity").toString("all");
        LOG_MESSAGE(QString("GetIssuesListTool: Severity filter: %1").arg(severityFilter));

        const auto tasks = IssuesTracker::instance().getTasks();

        if (tasks.isEmpty()) {
            LOG_MESSAGE("GetIssuesListTool: No issues found");
            return "No issues found in Qt Creator Issues panel.";
        }

        LOG_MESSAGE(QString("GetIssuesListTool: Processing %1 tasks").arg(tasks.size()));

        QStringList results;
        results.append(QString("Total issues in panel: %1\n").arg(tasks.size()));

        int errorCount = 0;
        int warningCount = 0;
        int processedCount = 0;

        for (const ProjectExplorer::Task &task : tasks) {
            if (severityFilter == "error" && task.type != ProjectExplorer::Task::Error)
                continue;
            if (severityFilter == "warning" && task.type != ProjectExplorer::Task::Warning)
                continue;

            QString typeStr;
            switch (task.type) {
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

            if (!task.file.isEmpty()) {
                issueText += QString("\n  File: %1").arg(task.file.toUrlishString());
                if (task.line > 0) {
                    issueText += QString(":%1").arg(task.line);
                    if (task.column > 0) {
                        issueText += QString(":%1").arg(task.column);
                    }
                }
            }

            if (!task.category.toString().isEmpty()) {
                issueText += QString("\n  Category: %1").arg(task.category.toString());
            }

            results.append(issueText);
            processedCount++;
        }

        QString summary = QString("\nSummary: %1 errors, %2 warnings (processed %3 tasks)")
                              .arg(errorCount)
                              .arg(warningCount)
                              .arg(processedCount);
        results.prepend(summary);

        LOG_MESSAGE(QString("GetIssuesListTool: Execution completed - %1 errors, %2 warnings")
                        .arg(errorCount)
                        .arg(warningCount));

        return results.join("\n\n");
    });
}

} // namespace QodeAssist::Tools
