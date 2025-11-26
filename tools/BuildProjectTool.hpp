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
    QSharedPointer<QPromise<QString>> promise;
    QPointer<ProjectExplorer::Project> project;
    QString projectName;
    bool isRebuild = false;
    bool runAfterBuild = false;
    QMetaObject::Connection buildFinishedConnection;
};

class BuildProjectTool : public LLMCore::BaseTool
{
    Q_OBJECT
public:
    explicit BuildProjectTool(QObject *parent = nullptr);
    ~BuildProjectTool() override;

    QString name() const override;
    QString stringName() const override;
    QString description() const override;
    QJsonObject getDefinition(LLMCore::ToolSchemaFormat format) const override;
    LLMCore::ToolPermissions requiredPermissions() const override;

    QFuture<QString> executeAsync(const QJsonObject &input = QJsonObject()) override;

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
