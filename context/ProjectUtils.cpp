// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProjectUtils.hpp"

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <utils/filepath.h>

namespace QodeAssist::Context {

bool ProjectUtils::isFileInProject(const QString &filePath)
{
    QList<ProjectExplorer::Project *> projects = ProjectExplorer::ProjectManager::projects();
    Utils::FilePath targetPath = Utils::FilePath::fromString(filePath);

    for (auto project : projects) {
        if (!project)
            continue;

        Utils::FilePaths projectFiles = project->files(ProjectExplorer::Project::SourceFiles);
        for (const auto &projectFile : std::as_const(projectFiles)) {
            if (projectFile == targetPath) {
                return true;
            }
        }

        Utils::FilePath projectDir = project->projectDirectory();
        if (targetPath.isChildOf(projectDir)) {
            return true;
        }
    }

    return false;
}

QString ProjectUtils::findFileInProject(const QString &filename)
{
    QList<ProjectExplorer::Project *> projects = ProjectExplorer::ProjectManager::projects();

    for (auto project : projects) {
        if (!project)
            continue;

        Utils::FilePaths projectFiles = project->files(ProjectExplorer::Project::SourceFiles);
        for (const auto &projectFile : std::as_const(projectFiles)) {
            if (projectFile.fileName() == filename) {
                return projectFile.toFSPathString();
            }
        }
    }

    return QString();
}

QString ProjectUtils::getProjectRoot()
{
    QList<ProjectExplorer::Project *> projects = ProjectExplorer::ProjectManager::projects();

    if (!projects.isEmpty()) {
        auto project = projects.first();
        if (project) {
            return project->projectDirectory().toFSPathString();
        }
    }

    return QString();
}

} // namespace QodeAssist::Context
