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

#include "FindFileTool.hpp"

#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent>

namespace QodeAssist::Tools {

FindFileTool::FindFileTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString FindFileTool::name() const
{
    return "find_file";
}

QString FindFileTool::stringName() const
{
    return {"Finding file in project"};
}

QString FindFileTool::description() const
{
    return "Search for files in the current project by filename, partial filename, or path "
           "(relative or absolute). "
           "This tool searches for files within the project scope and supports:\n"
           "- Exact filename match (e.g., 'main.cpp')\n"
           "- Partial filename match (e.g., 'main' will find 'main.cpp', 'main.h', etc.)\n"
           "- Relative path from project root (e.g., 'src/utils/helper.cpp')\n"
           "- Partial path matching (e.g., 'utils/helper' will find matching paths)\n"
           "- File extension filtering (e.g., '*.cpp', '*.h')\n"
           "- Case-insensitive search\n"
           "Input parameters:\n"
           "- 'query' (required): the filename, partial name, or path to search for\n"
           "- 'file_pattern' (optional): filter by file extension (e.g., '*.cpp', '*.h')\n"
           "- 'max_results' (optional): maximum number of results to return (default: 50)\n"
           "Returns a list of matching files with their absolute paths and relative paths from "
           "project root, "
           "or an error if no files are found or if the file is outside the project scope.";
}

QJsonObject FindFileTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;
    
    QJsonObject queryProperty;
    queryProperty["type"] = "string";
    queryProperty["description"]
        = "The filename, partial filename, or path to search for (case-insensitive)";
    properties["query"] = queryProperty;

    QJsonObject filePatternProperty;
    filePatternProperty["type"] = "string";
    filePatternProperty["description"]
        = "Optional file pattern to filter results (e.g., '*.cpp', '*.h', '*.qml')";
    properties["file_pattern"] = filePatternProperty;

    QJsonObject maxResultsProperty;
    maxResultsProperty["type"] = "integer";
    maxResultsProperty["description"]
        = "Maximum number of results to return (default: 50, max: 200)";
    maxResultsProperty["default"] = DEFAULT_MAX_RESULTS;
    properties["max_results"] = maxResultsProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;

    QJsonArray required;
    required.append("query");
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

LLMCore::ToolPermissions FindFileTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::FileSystemRead;
}

QFuture<QString> FindFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString query = input["query"].toString().trimmed();
        if (query.isEmpty()) {
            QString error = "Error: query parameter is required and cannot be empty";
            throw std::invalid_argument(error.toStdString());
        }

        QString filePattern = input["file_pattern"].toString().trimmed();
        int maxResults = input["max_results"].toInt(DEFAULT_MAX_RESULTS);
        
        maxResults = qBound(1, maxResults, 200);

        LOG_MESSAGE(QString("FindFileTool: Searching for '%1'%2 (max: %3)")
                        .arg(query)
                        .arg(
                            filePattern.isEmpty() ? QString("")
                                                  : QString(" with pattern '%1'").arg(filePattern))
                        .arg(maxResults));

        QFileInfo queryInfo(query);
        if (queryInfo.isAbsolute() && queryInfo.exists() && queryInfo.isFile()) {
            QString canonicalPath = queryInfo.canonicalFilePath();
            if (!isFileInProject(canonicalPath)) {
                QString error = QString("Error: File '%1' exists but is outside the project scope. "
                                        "Only files within the project can be accessed.")
                                    .arg(canonicalPath);
                throw std::runtime_error(error.toStdString());
            }

            auto project = ProjectExplorer::ProjectManager::projectForFile(
                Utils::FilePath::fromString(canonicalPath));

            if (project && !m_ignoreManager->shouldIgnore(canonicalPath, project)) {
                FileMatch match;
                match.absolutePath = canonicalPath;
                match.relativePath = QDir(project->projectDirectory().toFSPathString())
                                         .relativeFilePath(canonicalPath);
                match.projectName = project->displayName();
                match.matchType = FileMatch::PathMatch;
                
                QList<FileMatch> matches;
                matches.append(match);
                return formatResults(matches, 1, maxResults);
            }
        }

        QList<FileMatch> matches = findMatchingFiles(query, maxResults);
        int totalFound = matches.size();

        if (matches.isEmpty()) {
            QString error = QString(
                                "Error: No files found matching '%1'%2 in the project. "
                                "Try using a different search term or check the file name.")
                                .arg(query)
                                .arg(
                                    filePattern.isEmpty()
                                        ? QString("")
                                        : QString(" with pattern '%1'").arg(filePattern));
            throw std::runtime_error(error.toStdString());
        }

        return formatResults(matches, totalFound, maxResults);
    });
}

QList<FindFileTool::FileMatch> FindFileTool::findMatchingFiles(const QString &query,
                                                                int maxResults) const
{
    QList<FileMatch> matches;
    QList<ProjectExplorer::Project *> projects = ProjectExplorer::ProjectManager::projects();

    if (projects.isEmpty()) {
        LOG_MESSAGE("FindFileTool: No projects are currently open");
        return matches;
    }

    QString lowerQuery = query.toLower();

    for (auto project : projects) {
        if (!project)
            continue;

        Utils::FilePaths projectFiles = project->files(ProjectExplorer::Project::SourceFiles);
        Utils::FilePath projectDir = project->projectDirectory();
        QString projectName = project->displayName();

        for (const auto &filePath : projectFiles) {
            if (matches.size() >= maxResults) {
                break;
            }

            QString absolutePath = filePath.toFSPathString();

            if (m_ignoreManager->shouldIgnore(absolutePath, project)) {
                continue;
            }

            QFileInfo fileInfo(absolutePath);
            QString fileName = fileInfo.fileName();
            QString relativePath = QDir(projectDir.toFSPathString()).relativeFilePath(absolutePath);

            FileMatch match;
            match.absolutePath = absolutePath;
            match.relativePath = relativePath;
            match.projectName = projectName;

            if (fileName.toLower() == lowerQuery) {
                match.matchType = FileMatch::ExactName;
                matches.append(match);
                continue;
            }

            if (relativePath.toLower().contains(lowerQuery)) {
                match.matchType = FileMatch::PathMatch;
                matches.append(match);
                continue;
            }

            if (fileName.toLower().contains(lowerQuery)) {
                match.matchType = FileMatch::PartialName;
                matches.append(match);
                continue;
            }
        }

        if (matches.size() >= maxResults) {
            break;
        }
    }

    std::sort(matches.begin(), matches.end());

    return matches;
}

bool FindFileTool::matchesFilePattern(const QString &fileName, const QString &pattern) const
{
    if (pattern.isEmpty()) {
        return true;
    }

    if (pattern.startsWith("*.")) {
        QString extension = pattern.mid(1); // Remove the '*'
        return fileName.endsWith(extension, Qt::CaseInsensitive);
    }

    return fileName.compare(pattern, Qt::CaseInsensitive) == 0;
}

QString FindFileTool::formatResults(const QList<FileMatch> &matches,
                                    int totalFound,
                                    int maxResults) const
{
    QString result;
    bool wasTruncated = totalFound > matches.size();

    if (matches.size() == 1) {
        const FileMatch &match = matches.first();
        result = QString("Found 1 file:\n\n");
        result += QString("File: %1\n").arg(match.relativePath);
        result += QString("Absolute path: %2\n").arg(match.absolutePath);
        result += QString("Project: %3").arg(match.projectName);
    } else {
        result = QString("Found %1 file%2%3:\n\n")
                     .arg(totalFound)
                     .arg(totalFound == 1 ? QString("") : QString("s"))
                     .arg(wasTruncated ? QString(" (showing first %1)").arg(matches.size()) : "");

        QString currentProject;
        for (const FileMatch &match : matches) {
            if (currentProject != match.projectName) {
                if (!currentProject.isEmpty()) {
                    result += "\n";
                }
                result += QString("Project '%1':\n").arg(match.projectName);
                currentProject = match.projectName;
            }

            result += QString("  - %1\n").arg(match.relativePath);
            result += QString("    Absolute path: %2\n").arg(match.absolutePath);
        }

        if (wasTruncated) {
            result += QString(
                          "\n(Note: %1 additional file%2 not shown. "
                          "Use 'max_results' parameter to see more.)")
                          .arg(totalFound - matches.size())
                          .arg(totalFound - matches.size() == 1 ? QString("") : QString("s"));
        }
    }

    return result.trimmed();
}

bool FindFileTool::isFileInProject(const QString &filePath) const
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
