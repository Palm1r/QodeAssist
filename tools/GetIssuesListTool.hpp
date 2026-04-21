// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LLMQore/BaseTool.hpp>
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

class GetIssuesListTool : public ::LLMQore::BaseTool
{
    Q_OBJECT
public:
    explicit GetIssuesListTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;
};

} // namespace QodeAssist::Tools
