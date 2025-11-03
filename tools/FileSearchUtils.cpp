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

#include "FileSearchUtils.hpp"

#include <context/ProjectUtils.hpp>
#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <settings/GeneralSettings.hpp>
#include <settings/ToolsSettings.hpp>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace QodeAssist::Tools {

FileSearchUtils::FileMatch FileSearchUtils::findBestMatch(
    const QString &query,
    const QString &filePattern,
    int maxResults,
    Context::IgnoreManager *ignoreManager)
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
            
            if (ignoreManager && ignoreManager->shouldIgnore(absolutePath, project))
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
                depth,
                5,
                ignoreManager);
        }
    }

    if (candidates.isEmpty()) {
        return FileMatch{};
    }

    std::sort(candidates.begin(), candidates.end());
    return candidates.first();
}

void FileSearchUtils::searchInFileSystem(
    const QString &dirPath,
    const QString &query,
    const QString &projectName,
    const QString &projectDir,
    ProjectExplorer::Project *project,
    QList<FileMatch> &matches,
    int maxResults,
    int &currentDepth,
    int maxDepth,
    Context::IgnoreManager *ignoreManager)
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
        
        if (ignoreManager && ignoreManager->shouldIgnore(absolutePath, project))
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
                maxDepth,
                ignoreManager);
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

bool FileSearchUtils::matchesFilePattern(const QString &fileName, const QString &pattern)
{
    if (pattern.isEmpty())
        return true;

    if (pattern.startsWith("*.")) {
        QString extension = pattern.mid(1);
        return fileName.endsWith(extension, Qt::CaseInsensitive);
    }

    return fileName.compare(pattern, Qt::CaseInsensitive) == 0;
}

QString FileSearchUtils::readFileContent(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QString canonicalPath = QFileInfo(filePath).canonicalFilePath();
    bool isInProject = Context::ProjectUtils::isFileInProject(canonicalPath);

    if (!isInProject) {
        const auto &settings = Settings::toolsSettings();
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

} // namespace QodeAssist::Tools
