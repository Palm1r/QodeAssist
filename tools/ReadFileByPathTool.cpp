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

#include "ReadFileByPathTool.hpp"

#include <coreplugin/documentmanager.h>
#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QtConcurrent>

namespace QodeAssist::Tools {

ReadProjectFileByNameTool::ReadProjectFileByNameTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString ReadProjectFileByNameTool::name() const
{
    return "read_project_file_by_name";
}

QString ReadProjectFileByNameTool::stringName() const
{
    return {"Reading project file"};
}

QString ReadProjectFileByNameTool::description() const
{
    return "Read the content of a specific file from the current project by providing its "
           "absolute file path. "
           "The file must exist, be within the project scope, and not excluded by "
           ".qodeassistignore.\n"
           "Input parameter: 'filepath' - the absolute path to the file (e.g., "
           "'/path/to/project/src/main.cpp').\n"
           "Use 'list_project_files' tool first to get the exact file paths.";
}

QJsonObject ReadProjectFileByNameTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;
    QJsonObject filepathProperty;
    filepathProperty["type"] = "string";
    filepathProperty["description"] = "The absolute file path to read";
    properties["filepath"] = filepathProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;

    QJsonArray required;
    required.append("filepath");
    definition["required"] = required;

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

LLMCore::ToolPermissions ReadProjectFileByNameTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::FileSystemRead;
}

QFuture<QString> ReadProjectFileByNameTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString filePath = input["filepath"].toString();
        if (filePath.isEmpty()) {
            QString error = "Error: filepath parameter is required";
            throw std::invalid_argument(error.toStdString());
        }

        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            QString error = QString("Error: File '%1' does not exist").arg(filePath);
            throw std::runtime_error(error.toStdString());
        }

        QString canonicalPath = fileInfo.canonicalFilePath();

        if (!isFileInProject(canonicalPath)) {
            QString error = QString("Error: File '%1' is not part of the project").arg(filePath);
            throw std::runtime_error(error.toStdString());
        }

        auto project = ProjectExplorer::ProjectManager::projectForFile(
            Utils::FilePath::fromString(canonicalPath));
        if (project && m_ignoreManager->shouldIgnore(canonicalPath, project)) {
            QString error
                = QString("Error: File '%1' is excluded by .qodeassistignore").arg(filePath);
            throw std::runtime_error(error.toStdString());
        }

        QString content = readFileContent(canonicalPath);
        if (content.isNull()) {
            QString error = QString("Error: Could not read file '%1'").arg(canonicalPath);
            throw std::runtime_error(error.toStdString());
        }

        QString result = QString("File: %1\n\nContent:\n%2").arg(canonicalPath, content);
        return result;
    });
}

bool ReadProjectFileByNameTool::isFileInProject(const QString &filePath) const
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

QString ReadProjectFileByNameTool::readFileContent(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("Could not open file: %1").arg(filePath));
        return QString();
    }

    QTextStream stream(&file);
    stream.setAutoDetectUnicode(true);
    QString content = stream.readAll();

    file.close();
    return content;
}

} // namespace QodeAssist::Tools
