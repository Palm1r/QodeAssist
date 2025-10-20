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
#include <settings/GeneralSettings.hpp>
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
    return "Search for files in the project by filename, partial name, or path. "
           "Searches both in CMake-registered files and filesystem (finds .gitignore, Python scripts, README, etc.). "
           "Supports exact/partial filename match, relative/absolute paths, file extension filtering, "
           "and case-insensitive search. "
           "Returns matching files with absolute and relative paths.";
}

QJsonObject FindFileTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;
    
    QJsonObject queryProperty;
    queryProperty["type"] = "string";
    queryProperty["description"]
        = "The filename, partial filename, or path to search for (case-insensitive). "
          "Finds ALL files in project directory including .gitignore, README.md, Python scripts, "
          "config files, etc., even if not in CMake build system";
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
            bool isInProject = isFileInProject(canonicalPath);
            
            // Check if reading outside project is allowed
            if (!isInProject) {
                const auto &settings = Settings::generalSettings();
                if (!settings.allowReadOutsideProject()) {
                    QString error = QString("Error: File '%1' exists but is outside the project scope. "
                                            "Enable 'Allow reading files outside project' in settings to access this file.")
                                        .arg(canonicalPath);
                    throw std::runtime_error(error.toStdString());
                }
                LOG_MESSAGE(QString("Finding file outside project scope: %1").arg(canonicalPath));
            }

            auto project = isInProject ? ProjectExplorer::ProjectManager::projectForFile(
                Utils::FilePath::fromString(canonicalPath)) : nullptr;

            if (!isInProject || (project && !m_ignoreManager->shouldIgnore(canonicalPath, project))) {
                FileMatch match;
                match.absolutePath = canonicalPath;
                match.relativePath = isInProject && project 
                    ? QDir(project->projectDirectory().toFSPathString()).relativeFilePath(canonicalPath)
                    : canonicalPath;
                match.projectName = isInProject && project ? project->displayName() : "External";
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

    // If we didn't find enough matches in project files, search the filesystem
    if (matches.size() < maxResults) {
        LOG_MESSAGE(QString("FindFileTool: Extending search to filesystem (found %1 matches so far)")
                        .arg(matches.size()));
        
        for (auto project : projects) {
            if (!project)
                continue;
            
            if (matches.size() >= maxResults) {
                break;
            }
            
            Utils::FilePath projectDir = project->projectDirectory();
            QString projectName = project->displayName();
            QString projectDirStr = projectDir.toFSPathString();
            
            int depth = 0;
            searchInFileSystem(projectDirStr, lowerQuery, projectName, projectDirStr, 
                             project, matches, maxResults, depth);
        }
    }

    std::sort(matches.begin(), matches.end());

    return matches;
}

void FindFileTool::searchInFileSystem(const QString &dirPath,
                                       const QString &query,
                                       const QString &projectName,
                                       const QString &projectDir,
                                       ProjectExplorer::Project *project,
                                       QList<FileMatch> &matches,
                                       int maxResults,
                                       int &currentDepth,
                                       int maxDepth) const
{
    if (currentDepth > maxDepth || matches.size() >= maxResults) {
        return;
    }
    
    currentDepth++;
    
    QDir dir(dirPath);
    if (!dir.exists()) {
        currentDepth--;
        return;
    }
    
    // Get all entries (files and directories)
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    
    for (const QFileInfo &entry : entries) {
        if (matches.size() >= maxResults) {
            break;
        }
        
        QString absolutePath = entry.absoluteFilePath();
        
        // Check if should be ignored
        if (project && m_ignoreManager->shouldIgnore(absolutePath, project)) {
            continue;
        }
        
        // Skip common build/cache directories
        QString fileName = entry.fileName();
        if (entry.isDir()) {
            // Skip common build/cache directories
            if (fileName == "build" || fileName == ".git" || fileName == "node_modules" ||
                fileName == "__pycache__" || fileName == ".venv" || fileName == "venv" ||
                fileName == ".cmake" || fileName == "CMakeFiles" || fileName.startsWith(".qt")) {
                continue;
            }
            
            // Recurse into subdirectory
            searchInFileSystem(absolutePath, query, projectName, projectDir, 
                             project, matches, maxResults, currentDepth, maxDepth);
            continue;
        }
        
        // Check if already in matches (avoid duplicates from project files)
        bool alreadyAdded = false;
        for (const auto &match : matches) {
            if (match.absolutePath == absolutePath) {
                alreadyAdded = true;
                break;
            }
        }
        
        if (alreadyAdded) {
            continue;
        }
        
        // Match logic
        QString lowerFileName = fileName.toLower();
        QString relativePath = QDir(projectDir).relativeFilePath(absolutePath);
        QString lowerRelativePath = relativePath.toLower();
        
        FileMatch match;
        match.absolutePath = absolutePath;
        match.relativePath = relativePath;
        match.projectName = projectName;
        
        if (lowerFileName == query) {
            match.matchType = FileMatch::ExactName;
            matches.append(match);
        } else if (lowerRelativePath.contains(query)) {
            match.matchType = FileMatch::PathMatch;
            matches.append(match);
        } else if (lowerFileName.contains(query)) {
            match.matchType = FileMatch::PartialName;
            matches.append(match);
        }
    }
    
    currentDepth--;
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
