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

#include <llmcore/BaseTool.hpp>
#include <projectexplorer/task.h>
#include <QList>
#include <QMutex>

namespace QodeAssist::Tools {

class IssuesTracker : public QObject
{
    Q_OBJECT
public:
    static IssuesTracker &instance();

    QList<ProjectExplorer::Task> getTasks() const;

private:
    explicit IssuesTracker(QObject *parent = nullptr);
    ~IssuesTracker() override = default;

    void onTaskAdded(const ProjectExplorer::Task &task);
    void onTaskRemoved(const ProjectExplorer::Task &task);
    void onTasksCleared(Utils::Id categoryId);

    QList<ProjectExplorer::Task> m_tasks;
    mutable QMutex m_mutex;
};

class GetIssuesListTool : public LLMCore::BaseTool
{
    Q_OBJECT
public:
    explicit GetIssuesListTool(QObject *parent = nullptr);

    QString name() const override;
    QString stringName() const override;
    QString description() const override;
    QJsonObject getDefinition(LLMCore::ToolSchemaFormat format) const override;
    LLMCore::ToolPermissions requiredPermissions() const override;

    QFuture<QString> executeAsync(const QJsonObject &input = QJsonObject()) override;
};

} // namespace QodeAssist::Tools
