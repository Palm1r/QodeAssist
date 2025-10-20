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
#include <settings/GeneralSettings.hpp>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QtConcurrent>

namespace QodeAssist::Tools {

ReadProjectFileByPathTool::ReadProjectFileByPathTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString ReadProjectFileByPathTool::name() const
{
    return "read_project_file_by_path";
}

QString ReadProjectFileByPathTool::stringName() const
{
    return {"Reading project file"};
}

QString ReadProjectFileByPathTool::description() const
{
    return "Read content of a specific project file by its absolute path. "
           "File must exist, be within project scope, and not excluded by .qodeassistignore.";
}

QJsonObject ReadProjectFileByPathTool::getDefinition(LLMCore::ToolSchemaFormat format) const
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

LLMCore::ToolPermissions ReadProjectFileByPathTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::FileSystemRead;
}

QFuture<QString> ReadProjectFileByPathTool::executeAsync(const QJsonObject &input)
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

        bool isInProject = isFileInProject(canonicalPath);
        
        // Check if reading outside project is allowed
        if (!isInProject) {
            const auto &settings = Settings::generalSettings();
            if (!settings.allowReadOutsideProject()) {
                QString error = QString("Error: File '%1' is not part of the project. "
                                        "Enable 'Allow reading files outside project' in settings to access this file.")
                                    .arg(filePath);
                throw std::runtime_error(error.toStdString());
            }
            LOG_MESSAGE(QString("Reading file outside project scope: %1").arg(canonicalPath));
        }

        auto project = isInProject ? ProjectExplorer::ProjectManager::projectForFile(
            Utils::FilePath::fromString(canonicalPath)) : nullptr;
        if (isInProject && project && m_ignoreManager->shouldIgnore(canonicalPath, project)) {
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

bool ReadProjectFileByPathTool::isFileInProject(const QString &filePath) const
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

QString ReadProjectFileByPathTool::readFileContent(const QString &filePath) const
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
