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

#include <logger/Logger.hpp>
#include <projectexplorer/buildmanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>

namespace QodeAssist::Tools {

BuildProjectTool::BuildProjectTool(QObject *parent)
    : BaseTool(parent)
{
}

QString BuildProjectTool::name() const
{
    return "build_project";
}

QString BuildProjectTool::stringName() const
{
    return "Building project";
}

QString BuildProjectTool::description() const
{
    return "Build the current project in Qt Creator. "
           "Returns build status and any compilation errors/warnings. "
           "Optional 'rebuild' parameter: set to true to force a clean rebuild (default: false).";
}

QJsonObject BuildProjectTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject definition;
    definition["type"] = "object";

    QJsonObject properties;
    properties["rebuild"] = QJsonObject{
        {"type", "boolean"},
        {"description", "Force a clean rebuild instead of incremental build (default: false)"}};

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
        LOG_MESSAGE("BuildProjectTool: No active project found");
        return QtFuture::makeReadyFuture(
            QString("Error: No active project found. Please open a project in Qt Creator."));
    }

    LOG_MESSAGE(QString("BuildProjectTool: Active project is '%1'").arg(project->displayName()));

    if (ProjectExplorer::BuildManager::isBuilding(project)) {
        LOG_MESSAGE("BuildProjectTool: Build is already in progress");
        return QtFuture::makeReadyFuture(
            QString("Error: Build is already in progress. Please wait for it to complete."));
    }

    bool rebuild = input.value("rebuild").toBool(false);

    LOG_MESSAGE(QString("BuildProjectTool: Starting %1").arg(rebuild ? "rebuild" : "build"));

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

    return QtFuture::makeReadyFuture(
        QString("Build %1 started for project '%2'. Check the Compile Output pane for progress.")
            .arg(rebuild ? QString("rebuild") : QString("build"))
            .arg(project->displayName()));
}

} // namespace QodeAssist::Tools
