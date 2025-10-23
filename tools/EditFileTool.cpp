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

#include "EditFileTool.hpp"
#include "ToolExceptions.hpp"

#include <coreplugin/documentmanager.h>
#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <settings/GeneralSettings.hpp>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTextStream>
#include <QtConcurrent>

namespace QodeAssist::Tools {

EditFileTool::EditFileTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString EditFileTool::name() const
{
    return "edit_file";
}

QString EditFileTool::stringName() const
{
    return {"Editing file"};
}

QString EditFileTool::description() const
{
    return "Edit a project file by replacing text or appending content. "
           "Provide filename (absolute or relative path), search_text (text to find and replace, "
           "empty to append), "
           "and new_text (replacement or content to append). ";
}

QJsonObject EditFileTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;

    QJsonObject filepathProperty;
    filepathProperty["type"] = "string";
    filepathProperty["description"] = "The absolute or relative file path to edit";
    properties["filepath"] = filepathProperty;

    QJsonObject searchTextProperty;
    searchTextProperty["type"] = "string";
    searchTextProperty["description"]
        = "Text to search for and replace. If empty, new_text will be appended to the file";
    properties["search_text"] = searchTextProperty;

    QJsonObject newTextProperty;
    newTextProperty["type"] = "string";
    newTextProperty["description"] = "New text to replace search_text with, or content to append if search_text is empty";
    properties["new_text"] = newTextProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;

    QJsonArray required;
    required.append("filename");
    required.append("new_text");
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

LLMCore::ToolPermissions EditFileTool::requiredPermissions() const
{
    return LLMCore::ToolPermissions(LLMCore::ToolPermission::FileSystemRead) 
         | LLMCore::ToolPermissions(LLMCore::ToolPermission::FileSystemWrite);
}

QFuture<QString> EditFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString inputFilepath = input["filepath"].toString();
        if (inputFilepath.isEmpty()) {
            throw ToolInvalidArgument("Error: filename parameter is required");
        }

        QString newText = input["new_text"].toString();
        if (newText.isEmpty()) {
            throw ToolInvalidArgument("Error: new_text parameter is required");
        }

        QString searchText = input["search_text"].toString();

        QString filePath;
        QFileInfo fileInfo(inputFilepath);

        if (fileInfo.isAbsolute()) {
            filePath = inputFilepath;
        } else {
            auto projects = ProjectExplorer::ProjectManager::projects();
            if (!projects.isEmpty() && projects.first()) {
                QString projectDir = projects.first()->projectDirectory().toUrlishString();
                filePath = QDir(projectDir).absoluteFilePath(inputFilepath);
            } else {
                filePath = QFileInfo(inputFilepath).absoluteFilePath();
            }
        }

        if (!QFileInfo::exists(filePath)) {
            throw ToolRuntimeError(QString("Error: File '%1' does not exist").arg(filePath));
        }

        bool isInProject = isFileInProject(filePath);
        
        if (!isInProject) {
            const auto &settings = Settings::generalSettings();
            if (!settings.allowReadOutsideProject()) {
                throw ToolRuntimeError(
                    QString("Error: File '%1' is outside the project scope. "
                            "Enable 'Allow reading files outside project' in settings to access this file.")
                        .arg(filePath));
            }
            LOG_MESSAGE(QString("Editing file outside project scope: %1").arg(filePath));
        }

        auto project = isInProject ? ProjectExplorer::ProjectManager::projectForFile(
            Utils::FilePath::fromString(filePath)) : nullptr;
        
        if (project && m_ignoreManager->shouldIgnore(filePath, project)) {
            throw ToolRuntimeError(
                QString("Error: File '%1' is excluded by .qodeassistignore").arg(inputFilepath));
        }

        QJsonObject result;
        result["type"] = "file_edit";
        result["filepath"] = filePath;
        result["new_text"] = newText;
        result["search_text"] = searchText;

        QJsonDocument doc(result);
        return QString("QODEASSIST_FILE_EDIT:%1")
            .arg(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    });
}

bool EditFileTool::isFileInProject(const QString &filePath) const
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

} // namespace QodeAssist::Tools

