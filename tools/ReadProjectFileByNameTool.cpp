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

#include "ReadProjectFileByNameTool.hpp"

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

QString ReadProjectFileByNameTool::description() const
{
    return "Read the content of a specific file from the current project by providing its filename "
           "or "
           "relative path. This tool searches for files within the project scope and supports:\n"
           "- Exact filename match (e.g., 'main.cpp')\n"
           "- Relative path from project root (e.g., 'src/utils/helper.cpp')\n"
           "- Partial path matching (e.g., 'utils/helper.cpp')\n"
           "- Case-insensitive filename search as fallback\n"
           "Input parameter: 'filename' - the name or path of the file to read.\n"
           "Use this when you need to examine specific project files that are not currently open "
           "in the editor.";
}

QJsonObject ReadProjectFileByNameTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;
    QJsonObject filenameProperty;
    filenameProperty["type"] = "string";
    filenameProperty["description"] = "The filename or relative path to read";
    properties["filename"] = filenameProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;

    QJsonArray required;
    required.append("filename");
    definition["required"] = required;

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

QFuture<QString> ReadProjectFileByNameTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString filename = input["filename"].toString();
        if (filename.isEmpty()) {
            QString error = "Error: filename parameter is required";
            throw std::invalid_argument(error.toStdString());
        }

        QString filePath = findFileInProject(filename);
        if (filePath.isEmpty()) {
            QString error = QString("Error: File '%1' not found").arg(filename);
            throw std::runtime_error(error.toStdString());
        }

        auto project = ProjectExplorer::ProjectManager::projectForFile(
            Utils::FilePath::fromString(filePath));
        if (project && m_ignoreManager->shouldIgnore(filePath, project)) {
            QString error
                = QString("Error: File '%1' is excluded by .qodeassistignore").arg(filename);
            throw std::runtime_error(error.toStdString());
        }

        QString content = readFileContent(filePath);
        if (content.isNull()) {
            QString error = QString("Error: Could not read file '%1'").arg(filePath);
            throw std::runtime_error(error.toStdString());
        }

        QString result = QString("File: %1\n\nContent:\n%2").arg(filePath, content);
        return result;
    });
}

QString ReadProjectFileByNameTool::findFileInProject(const QString &fileName) const
{
    QList<ProjectExplorer::Project *> projects = ProjectExplorer::ProjectManager::projects();
    if (projects.isEmpty()) {
        LOG_MESSAGE("No projects found");
        return QString();
    }

    for (auto project : projects) {
        if (!project)
            continue;

        Utils::FilePaths projectFiles = project->files(ProjectExplorer::Project::SourceFiles);

        for (const auto &projectFile : std::as_const(projectFiles)) {
            QString absolutePath = projectFile.path();

            if (m_ignoreManager->shouldIgnore(absolutePath, project)) {
                continue;
            }

            QFileInfo fileInfo(absolutePath);
            if (fileInfo.fileName() == fileName) {
                return absolutePath;
            }
        }

        for (const auto &projectFile : std::as_const(projectFiles)) {
            QString absolutePath = projectFile.path();

            if (m_ignoreManager->shouldIgnore(absolutePath, project)) {
                continue;
            }

            if (projectFile.endsWith(fileName)) {
                return absolutePath;
            }
        }

        for (const auto &projectFile : std::as_const(projectFiles)) {
            QString absolutePath = projectFile.path();

            if (m_ignoreManager->shouldIgnore(absolutePath, project)) {
                continue;
            }

            QFileInfo fileInfo(absolutePath);
            if (fileInfo.fileName().contains(fileName, Qt::CaseInsensitive)) {
                return absolutePath;
            }
        }
    }

    return QString();
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
