// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LLMQore/BaseTool.hpp>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QPromise>
#include <QSharedPointer>

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::Tools {

struct BuildInfo
{
    QSharedPointer<QPromise<LLMQore::ToolResult>> promise;
    QPointer<ProjectExplorer::Project> project;
    QString projectName;
    bool isRebuild = false;
    bool runAfterBuild = false;
    QMetaObject::Connection buildFinishedConnection;
};

class BuildProjectTool : public ::LLMQore::BaseTool
{
    Q_OBJECT
public:
    explicit BuildProjectTool(QObject *parent = nullptr);
    ~BuildProjectTool() override;

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;

private slots:
    void onBuildQueueFinished(bool success);

private:
    void scheduleProjectRun(ProjectExplorer::Project *project,
                            const QString &projectName,
                            QString &result);
    QString collectBuildResults(bool success, const QString &projectName, bool isRebuild);
    void cleanupBuildInfo(ProjectExplorer::Project *project);

    QHash<ProjectExplorer::Project *, BuildInfo> m_activeBuilds;
};

} // namespace QodeAssist::Tools
