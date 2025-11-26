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

#include "BuildProjectTool.hpp"

#include "GetIssuesListTool.hpp"

#include <Version.hpp>
#include <logger/Logger.hpp>
#include <projectexplorer/buildmanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/target.h>
#include <projectexplorer/task.h>
#include <utils/id.h>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QTimer>

namespace QodeAssist::Tools {

BuildProjectTool::BuildProjectTool(QObject *parent)
    : BaseTool(parent)
{
}

BuildProjectTool::~BuildProjectTool()
{
    for (auto it = m_activeBuilds.begin(); it != m_activeBuilds.end(); ++it) {
        BuildInfo &info = it.value();
        if (info.buildFinishedConnection) {
            disconnect(info.buildFinishedConnection);
        }
        if (info.promise) {
            info.promise->finish();
        }
    }
    m_activeBuilds.clear();
}

QString BuildProjectTool::name() const
{
    return "build_project";
}

QString BuildProjectTool::stringName() const
{
    return "Building and running project";
}

QString BuildProjectTool::description() const
{
    return "Build the current project in Qt Creator and wait for completion. "
           "Optionally run the project after successful build. "
           "Returns build status (success/failure) and any compilation errors/warnings after "
           "the build finishes. "
           "Optional 'rebuild' parameter: set to true to force a clean rebuild (default: false). "
           "Optional 'run_after_build' parameter: set to true to run the project after successful build (default: false). "
           "Note: This operation may take some time depending on project size.";
}

QJsonObject BuildProjectTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject definition;
    definition["type"] = "object";

    QJsonObject properties;
    properties["rebuild"] = QJsonObject{
        {"type", "boolean"},
        {"description", "Force a clean rebuild instead of incremental build (default: false)"}};
    properties["run_after_build"] = QJsonObject{
        {"type", "boolean"},
        {"description", "Run the project after successful build (default: false)"}};

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

LLMCore::ToolPermissions BuildProjectTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::None;
}

QFuture<QString> BuildProjectTool::executeAsync(const QJsonObject &input)
{
    auto *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project) {
        return QtFuture::makeReadyFuture(
            QString("Error: No active project found. Please open a project in Qt Creator."));
    }

    if (ProjectExplorer::BuildManager::isBuilding(project)) {
        return QtFuture::makeReadyFuture(
            QString("Error: Build is already in progress. Please wait for it to complete."));
    }

    if (m_activeBuilds.contains(project)) {
        return QtFuture::makeReadyFuture(
            QString("Error: Build is already being tracked for project '%1'.")
                .arg(project->displayName()));
    }

    bool rebuild = input.value("rebuild").toBool(false);
    bool runAfterBuild = input.value("run_after_build").toBool(false);

    LOG_MESSAGE(QString("BuildProjectTool: %1 project '%2'%3")
                    .arg(rebuild ? QString("Rebuilding") : QString("Building"))
                    .arg(project->displayName())
                    .arg(runAfterBuild ? QString(" (run after build)") : QString()));

    auto promise = QSharedPointer<QPromise<QString>>::create();
    promise->start();

    BuildInfo buildInfo;
    buildInfo.promise = promise;
    buildInfo.project = project;
    buildInfo.projectName = project->displayName();
    buildInfo.isRebuild = rebuild;
    buildInfo.runAfterBuild = runAfterBuild;

    auto *buildManager = ProjectExplorer::BuildManager::instance();
    buildInfo.buildFinishedConnection = QObject::connect(
        buildManager,
        &ProjectExplorer::BuildManager::buildQueueFinished,
        this,
        &BuildProjectTool::onBuildQueueFinished);

    m_activeBuilds.insert(project, buildInfo);

    QMetaObject::invokeMethod(
        qApp,
        [project, rebuild]() {
            if (rebuild) {
                ProjectExplorer::BuildManager::rebuildProjectWithDependencies(
                    project, ProjectExplorer::ConfigSelection::Active);
            } else {
                ProjectExplorer::BuildManager::buildProjectWithDependencies(
                    project, ProjectExplorer::ConfigSelection::Active);
            }
        },
        Qt::QueuedConnection);

    return promise->future();
}

void BuildProjectTool::onBuildQueueFinished(bool success)
{
    QList<ProjectExplorer::Project *> projectsToCleanup;

    for (auto it = m_activeBuilds.begin(); it != m_activeBuilds.end(); ++it) {
        ProjectExplorer::Project *project = it.key();

        if (!ProjectExplorer::BuildManager::isBuilding(project)) {
            BuildInfo &info = it.value();

            if (info.promise && info.promise->future().isCanceled()) {
                LOG_MESSAGE(QString("BuildProjectTool: Build cancelled for project '%1'")
                                .arg(info.projectName));
                projectsToCleanup.append(project);
                continue;
            }

            QString result = collectBuildResults(success, info.projectName, info.isRebuild);

            if (success && info.runAfterBuild) {
                scheduleProjectRun(project, info.projectName, result);
            } else if (!success && info.runAfterBuild) {
                result += QString("\n\nProject was not started due to build failure.");
            }

            if (info.promise) {
                info.promise->addResult(result);
                info.promise->finish();
            }

            projectsToCleanup.append(project);
        }
    }

    for (ProjectExplorer::Project *project : projectsToCleanup) {
        cleanupBuildInfo(project);
    }
}

void BuildProjectTool::scheduleProjectRun(ProjectExplorer::Project *project,
                                          const QString &projectName,
                                          QString &result)
{
    auto *target = project->activeTarget();
    if (!target) {
        result += QString("\n\nError: No active target found for the project.");
        return;
    }

    auto *runConfig = target->activeRunConfiguration();
    if (!runConfig) {
        result += QString("\n\nError: No active run configuration found for the project.");
        return;
    }

    QString runConfigName = runConfig->displayName();
    result += QString("\n\nProject '%1' will be started with run configuration '%2'.")
                  .arg(projectName, runConfigName);

    ProjectExplorer::ProjectExplorerPlugin::runProject(project, Utils::Id(ProjectExplorer::Constants::NORMAL_RUN_MODE));
}

QString BuildProjectTool::collectBuildResults(
    bool success, const QString &projectName, bool isRebuild)
{
    QStringList results;

    // Build header
    QString buildType = isRebuild ? QString("Rebuild") : QString("Build");
    QString statusText = success ? QString("✓ SUCCEEDED") : QString("✗ FAILED");

    results.append(QString("%1 %2 for project '%3'\n")
                       .arg(buildType, statusText, projectName));

    const auto tasks = IssuesTracker::instance().getTasks();

    if (!tasks.isEmpty()) {
        int errorCount = 0;
        int warningCount = 0;
        QStringList issuesList;

        for (const ProjectExplorer::Task &task : tasks) {
#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(18, 0, 0)
            auto taskType = task.type();
            auto taskFile = task.file();
            auto taskLine = task.line();
            auto taskColumn = task.column();
#else
            auto taskType = task.type;
            auto taskFile = task.file;
            auto taskLine = task.line;
            auto taskColumn = task.column;
#endif

            QString typeStr;
            switch (taskType) {
            case ProjectExplorer::Task::Error:
                typeStr = QString("ERROR");
                errorCount++;
                break;
            case ProjectExplorer::Task::Warning:
                typeStr = QString("WARNING");
                warningCount++;
                break;
            default:
                continue;
            }

            if (issuesList.size() < 50) {
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

                issuesList.append(issueText);
            }
        }

        results.append(QString("Issues found: %1 error(s), %2 warning(s)")
                           .arg(errorCount)
                           .arg(warningCount));

        if (!issuesList.isEmpty()) {
            results.append("\nDetails:");
            results.append(issuesList.join("\n\n"));

            if (errorCount + warningCount > 50) {
                results.append(
                    QString("\n... and %1 more issue(s). Use get_issues_list tool for full list.")
                        .arg(errorCount + warningCount - 50));
            }
        }
    } else {
        results.append("No compilation errors or warnings.");
    }

    return results.join("\n");
}

void BuildProjectTool::cleanupBuildInfo(ProjectExplorer::Project *project)
{
    if (!m_activeBuilds.contains(project)) {
        return;
    }

    BuildInfo info = m_activeBuilds.take(project);

    if (info.buildFinishedConnection) {
        disconnect(info.buildFinishedConnection);
    }
}

} // namespace QodeAssist::Tools
