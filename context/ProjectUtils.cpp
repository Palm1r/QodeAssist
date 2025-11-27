
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
