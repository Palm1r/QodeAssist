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

#include "FindAndReadFileTool.hpp"
#include "ToolExceptions.hpp"

#include <context/ProjectUtils.hpp>
#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <settings/GeneralSettings.hpp>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QtConcurrent>

namespace QodeAssist::Tools {

FindAndReadFileTool::FindAndReadFileTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString FindAndReadFileTool::name() const
{
    return "find_and_read_file";
}

QString FindAndReadFileTool::stringName() const
{
    return "Finding and reading file";
}

QString FindAndReadFileTool::description() const
{
    return "Search for a file by name/path and optionally read its content. "
           "Returns the best matching file and its content.";
}

QJsonObject FindAndReadFileTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;

    properties["query"] = QJsonObject{
        {"type", "string"},
        {"description", "Filename, partial name, or path to search for (case-insensitive)"}};

    properties["file_pattern"] = QJsonObject{
        {"type", "string"}, {"description", "File pattern filter (e.g., '*.cpp', '*.h', '*.qml')"}};

    properties["read_content"] = QJsonObject{
        {"type", "boolean"},
        {"description", "Read file content in addition to finding path (default: true)"}};

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    definition["required"] = QJsonArray{"query"};

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

LLMCore::ToolPermissions FindAndReadFileTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::FileSystemRead;
}

QFuture<QString> FindAndReadFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString query = input["query"].toString().trimmed();
        if (query.isEmpty()) {
            throw ToolInvalidArgument("Query parameter is required");
        }

        QString filePattern = input["file_pattern"].toString();
        bool readContent = input["read_content"].toBool(true);

        LOG_MESSAGE(QString("FindAndReadFileTool: Searching for '%1' (pattern: %2, read: %3)")
                        .arg(query, filePattern.isEmpty() ? "none" : filePattern)
                        .arg(readContent));

        FileMatch bestMatch = findBestMatch(query, filePattern, 10);

        if (bestMatch.absolutePath.isEmpty()) {
            return QString("No file found matching '%1'").arg(query);
        }

        if (readContent) {
            bestMatch.content = readFileContent(bestMatch.absolutePath);
            if (bestMatch.content.isNull()) {
                bestMatch.error = "Could not read file";
            }
        }

        return formatResult(bestMatch, readContent);
    });
}

FindAndReadFileTool::FileMatch FindAndReadFileTool::findBestMatch(
    const QString &query, const QString &filePattern, int maxResults)
{
    QList<FileMatch> candidates;
    auto projects = ProjectExplorer::ProjectManager::projects();

    if (projects.isEmpty()) {
        return FileMatch{};
    }

    QFileInfo queryInfo(query);
    if (queryInfo.isAbsolute() && queryInfo.exists() && queryInfo.isFile()) {
        FileMatch match;
        match.absolutePath = queryInfo.canonicalFilePath();

        for (auto project : projects) {
            if (!project)
                continue;
            QString projectDir = project->projectDirectory().path();
            if (match.absolutePath.startsWith(projectDir)) {
                match.relativePath = QDir(projectDir).relativeFilePath(match.absolutePath);
                match.projectName = project->displayName();
                match.matchType = MatchType::ExactName;
                return match;
            }
        }

        match.relativePath = queryInfo.fileName();
        match.projectName = "External";
        match.matchType = MatchType::ExactName;
        return match;
    }

    QString lowerQuery = query.toLower();

    for (auto project : projects) {
        if (!project)
            continue;

        auto projectFiles = project->files(ProjectExplorer::Project::SourceFiles);
        QString projectDir = project->projectDirectory().path();
        QString projectName = project->displayName();

        for (const auto &filePath : projectFiles) {
            QString absolutePath = filePath.path();
            if (m_ignoreManager->shouldIgnore(absolutePath, project))
                continue;

            QFileInfo fileInfo(absolutePath);
            QString fileName = fileInfo.fileName();

            if (!filePattern.isEmpty() && !matchesFilePattern(fileName, filePattern))
                continue;

            QString relativePath = QDir(projectDir).relativeFilePath(absolutePath);

            FileMatch match;
            match.absolutePath = absolutePath;
            match.relativePath = relativePath;
            match.projectName = projectName;

            QString lowerFileName = fileName.toLower();
            QString lowerRelativePath = relativePath.toLower();

            if (lowerFileName == lowerQuery) {
                match.matchType = MatchType::ExactName;
                candidates.append(match);
            } else if (lowerRelativePath.contains(lowerQuery)) {
                match.matchType = MatchType::PathMatch;
                candidates.append(match);
            } else if (lowerFileName.contains(lowerQuery)) {
                match.matchType = MatchType::PartialName;
                candidates.append(match);
            }
        }
    }

    if (candidates.isEmpty() || candidates.first().matchType != MatchType::ExactName) {
        for (auto project : projects) {
            if (!project)
                continue;

            QString projectDir = project->projectDirectory().path();
            QString projectName = project->displayName();
            int depth = 0;
            searchInFileSystem(
                projectDir,
                lowerQuery,
                projectName,
                projectDir,
                project,
                candidates,
                maxResults,
                depth);
        }
    }

    if (candidates.isEmpty()) {
        return FileMatch{};
    }

    std::sort(candidates.begin(), candidates.end());
    return candidates.first();
}

void FindAndReadFileTool::searchInFileSystem(
    const QString &dirPath,
    const QString &query,
    const QString &projectName,
    const QString &projectDir,
    ProjectExplorer::Project *project,
    QList<FileMatch> &matches,
    int maxResults,
    int &currentDepth,
    int maxDepth)
{
    if (currentDepth >= maxDepth || matches.size() >= maxResults)
        return;

    currentDepth++;
    QDir dir(dirPath);
    if (!dir.exists()) {
        currentDepth--;
        return;
    }

    auto entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        if (matches.size() >= maxResults)
            break;

        QString absolutePath = entry.absoluteFilePath();
        if (m_ignoreManager->shouldIgnore(absolutePath, project))
            continue;

        QString fileName = entry.fileName();

        if (entry.isDir()) {
            searchInFileSystem(
                absolutePath,
                query,
                projectName,
                projectDir,
                project,
                matches,
                maxResults,
                currentDepth,
                maxDepth);
            continue;
        }

        QString lowerFileName = fileName.toLower();
        QString relativePath = QDir(projectDir).relativeFilePath(absolutePath);
        QString lowerRelativePath = relativePath.toLower();

        FileMatch match;
        match.absolutePath = absolutePath;
        match.relativePath = relativePath;
        match.projectName = projectName;

        if (lowerFileName == query) {
            match.matchType = MatchType::ExactName;
            matches.append(match);
        } else if (lowerRelativePath.contains(query)) {
            match.matchType = MatchType::PathMatch;
            matches.append(match);
        } else if (lowerFileName.contains(query)) {
            match.matchType = MatchType::PartialName;
            matches.append(match);
        }
    }

    currentDepth--;
}

bool FindAndReadFileTool::matchesFilePattern(const QString &fileName, const QString &pattern) const
{
    if (pattern.isEmpty())
        return true;

    if (pattern.startsWith("*.")) {
        QString extension = pattern.mid(1);
        return fileName.endsWith(extension, Qt::CaseInsensitive);
    }

    return fileName.compare(pattern, Qt::CaseInsensitive) == 0;
}

QString FindAndReadFileTool::readFileContent(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QString canonicalPath = QFileInfo(filePath).canonicalFilePath();
    bool isInProject = Context::ProjectUtils::isFileInProject(canonicalPath);

    if (!isInProject) {
        const auto &settings = Settings::generalSettings();
        if (!settings.allowAccessOutsideProject()) {
            LOG_MESSAGE(QString("Access denied to file outside project: %1").arg(canonicalPath));
            return QString();
        }
        LOG_MESSAGE(QString("Reading file outside project scope: %1").arg(canonicalPath));
    }

    QTextStream stream(&file);
    stream.setAutoDetectUnicode(true);
    QString content = stream.readAll();

    return content;
}

QString FindAndReadFileTool::formatResult(const FileMatch &match, bool readContent) const
{
    QString result
        = QString("Found file: %1\nAbsolute path: %2").arg(match.relativePath, match.absolutePath);

    if (readContent) {
        if (!match.error.isEmpty()) {
            result += QString("\nError: %1").arg(match.error);
        } else {
            result += QString("\n\n=== Content ===\n%1").arg(match.content);
        }
    }

    return result;
}

} // namespace QodeAssist::Tools
