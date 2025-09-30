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

#include "ListProjectFilesTool.hpp"

#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent>

namespace QodeAssist::Tools {

ListProjectFilesTool::ListProjectFilesTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))

{}

QString ListProjectFilesTool::name() const
{
    return "list_project_files";
}

QString ListProjectFilesTool::description() const
{
    return "Get a list of all source files in the current project. "
           "Returns a structured list of files with their relative paths from the project root. "
           "Useful for understanding project structure and finding specific files. "
           "No parameters required.";
}

QJsonObject ListProjectFilesTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = QJsonObject();
    definition["required"] = QJsonArray();

    switch (format) {
    case LLMCore::ToolSchemaFormat::OpenAI:
        return customizeForOpenAI(definition);
    case LLMCore::ToolSchemaFormat::Claude:
        return customizeForClaude(definition);
    case LLMCore::ToolSchemaFormat::Ollama:
        return customizeForOllama(definition);
    }

    return definition;
}

QFuture<QString> ListProjectFilesTool::executeAsync(const QJsonObject &input)
{
    Q_UNUSED(input)

    return QtConcurrent::run([this]() -> QString {
        QList<ProjectExplorer::Project *> projects = ProjectExplorer::ProjectManager::projects();
        if (projects.isEmpty()) {
            QString error = "Error: No projects found";
            throw std::runtime_error(error.toStdString());
        }

        QString result;

        for (auto project : projects) {
            if (!project)
                continue;

            Utils::FilePaths projectFiles = project->files(ProjectExplorer::Project::SourceFiles);

            if (projectFiles.isEmpty()) {
                result += QString("Project '%1': No source files found\n\n")
                              .arg(project->displayName());
                continue;
            }

            QStringList fileList;
            QString projectPath = project->projectDirectory().toUrlishString();

            for (const auto &filePath : projectFiles) {
                QString absolutePath = filePath.toUrlishString();

                if (m_ignoreManager->shouldIgnore(absolutePath, project)) {
                    LOG_MESSAGE(
                        QString("Ignoring file due to .qodeassistignore: %1").arg(absolutePath));
                    continue;
                }

                QString relativePath = QDir(projectPath).relativeFilePath(absolutePath);
                fileList.append(relativePath);
            }

            if (fileList.isEmpty()) {
                result += QString("Project '%1': No files after applying .qodeassistignore\n\n")
                              .arg(project->displayName());
                continue;
            }

            fileList.sort();

            result += QString("Project '%1' (%2 files):\n")
                          .arg(project->displayName())
                          .arg(fileList.size());
            for (const QString &file : fileList) {
                result += QString("- %1\n").arg(file);
            }
            result += "\n";
        }

        return result.trimmed();
    });
}

QString ListProjectFilesTool::formatFileList(const QStringList &files) const
{
    QString result = QString("Project files (%1 total):\n\n").arg(files.size());

    for (const QString &file : files) {
        result += QString("- %1\n").arg(file);
    }

    return result;
}

} // namespace QodeAssist::Tools
