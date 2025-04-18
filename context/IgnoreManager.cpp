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

#include "IgnoreManager.hpp"

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include "logger/Logger.hpp"

namespace QodeAssist::Context {

IgnoreManager::IgnoreManager(QObject *parent)
    : QObject(parent)
{
    auto projectManager = ProjectExplorer::ProjectManager::instance();
    if (projectManager) {
        connect(
            projectManager,
            &ProjectExplorer::ProjectManager::projectAdded,
            this,
            &IgnoreManager::reloadIgnorePatterns);

        connect(
            projectManager,
            &ProjectExplorer::ProjectManager::projectRemoved,
            this,
            &IgnoreManager::removeIgnorePatterns);

        const QList<ProjectExplorer::Project *> projects = projectManager->projects();
        for (ProjectExplorer::Project *project : projects) {
            if (project) {
                reloadIgnorePatterns(project);
            }
        }
    }

    connect(
        QCoreApplication::instance(),
        &QCoreApplication::aboutToQuit,
        this,
        &IgnoreManager::cleanupConnections);
}

IgnoreManager::~IgnoreManager()
{
    cleanupConnections();
}

void IgnoreManager::cleanupConnections()
{
    QList<ProjectExplorer::Project *> projects = m_projectConnections.keys();
    for (ProjectExplorer::Project *project : projects) {
        if (project) {
            disconnect(m_projectConnections.take(project));
        }
    }
    m_projectConnections.clear();
    m_projectIgnorePatterns.clear();
    m_ignoreCache.clear();
}

bool IgnoreManager::shouldIgnore(const QString &filePath, ProjectExplorer::Project *project) const
{
    if (!project)
        return false;

    if (!m_projectIgnorePatterns.contains(project)) {
        const_cast<IgnoreManager *>(this)->reloadIgnorePatterns(project);
    }

    const QStringList &patterns = m_projectIgnorePatterns[project];
    if (patterns.isEmpty())
        return false;

    QDir projectDir(project->projectDirectory().toUrlishString());
    QString relativePath = projectDir.relativeFilePath(filePath);

    return matchesIgnorePatterns(relativePath, patterns);
}

bool IgnoreManager::matchesIgnorePatterns(const QString &path, const QStringList &patterns) const
{
    QString cacheKey = path + ":" + patterns.join("|");
    if (m_ignoreCache.contains(cacheKey))
        return m_ignoreCache[cacheKey];

    bool result = isPathExcluded(path, patterns);
    m_ignoreCache.insert(cacheKey, result);
    return result;
}

bool IgnoreManager::isPathExcluded(const QString &path, const QStringList &patterns) const
{
    bool excluded = false;

    for (const QString &pattern : patterns) {
        if (pattern.isEmpty() || pattern.startsWith('#'))
            continue;

        bool isNegative = pattern.startsWith('!');
        QString actualPattern = isNegative ? pattern.mid(1) : pattern;

        bool matches = matchPathWithPattern(path, actualPattern);

        if (matches) {
            excluded = !isNegative;
        }
    }

    return excluded;
}

bool IgnoreManager::matchPathWithPattern(const QString &path, const QString &pattern) const
{
    QString adjustedPattern = pattern.trimmed();

    bool matchFromRoot = adjustedPattern.startsWith('/');
    if (matchFromRoot)
        adjustedPattern = adjustedPattern.mid(1);

    bool matchDirOnly = adjustedPattern.endsWith('/');
    if (matchDirOnly)
        adjustedPattern.chop(1);

    QString regexPattern = QRegularExpression::escape(adjustedPattern);

    regexPattern.replace("\\*\\*", ".*");

    regexPattern.replace("\\*", "[^/]*");

    regexPattern.replace("\\?", ".");

    if (matchFromRoot)
        regexPattern = QString("^%1").arg(regexPattern);
    else
        regexPattern = QString("(^|/)%1").arg(regexPattern);

    if (matchDirOnly)
        regexPattern = QString("%1$").arg(regexPattern);
    else
        regexPattern = QString("%1($|/)").arg(regexPattern);

    QRegularExpression regex(regexPattern);
    QRegularExpressionMatch match = regex.match(path);
    return match.hasMatch();
}

QStringList IgnoreManager::loadIgnorePatterns(ProjectExplorer::Project *project)
{
    QStringList patterns;
    if (!project)
        return patterns;

    QString ignoreFile = ignoreFilePath(project);
    if (ignoreFile.isEmpty() || !QFile::exists(ignoreFile)) {
        // LOG_MESSAGE(
        //     QString("No .qodeassistignore file found for project: %1").arg(project->displayName()));
        return patterns;
    }

    QFile file(ignoreFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("Could not open .qodeassistignore file: %1").arg(ignoreFile));
        return patterns;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith('#'))
            patterns << line;
    }

    LOG_MESSAGE(QString("Successfully loaded .qodeassistignore file: %1 with %2 patterns")
                    .arg(ignoreFile)
                    .arg(patterns.size()));

    return patterns;
}

void IgnoreManager::reloadIgnorePatterns(ProjectExplorer::Project *project)
{
    if (!project)
        return;

    QStringList patterns = loadIgnorePatterns(project);
    m_projectIgnorePatterns[project] = patterns;

    QStringList keysToRemove;
    QString projectPath = project->projectDirectory().toUrlishString();
    for (auto it = m_ignoreCache.begin(); it != m_ignoreCache.end(); ++it) {
        if (it.key().contains(projectPath))
            keysToRemove << it.key();
    }

    for (const QString &key : keysToRemove)
        m_ignoreCache.remove(key);

    if (!m_projectConnections.contains(project)) {
        QPointer<ProjectExplorer::Project> projectPtr(project);
        auto connection = connect(project, &QObject::destroyed, this, [this, projectPtr]() {
            if (projectPtr) {
                m_projectIgnorePatterns.remove(projectPtr);
                m_projectConnections.remove(projectPtr);

                QStringList keysToRemove;
                for (auto it = m_ignoreCache.begin(); it != m_ignoreCache.end(); ++it) {
                    if (it.key().contains(projectPtr->projectDirectory().toUrlishString()))
                        keysToRemove << it.key();
                }

                for (const QString &key : keysToRemove)
                    m_ignoreCache.remove(key);
            }
        });

        m_projectConnections[project] = connection;
    }
}

void IgnoreManager::removeIgnorePatterns(ProjectExplorer::Project *project)
{
    m_projectIgnorePatterns.remove(project);

    QStringList keysToRemove;
    for (auto it = m_ignoreCache.begin(); it != m_ignoreCache.end(); ++it) {
        if (it.key().contains(project->projectDirectory().toUrlishString()))
            keysToRemove << it.key();
    }

    for (const QString &key : keysToRemove)
        m_ignoreCache.remove(key);

    if (m_projectConnections.contains(project)) {
        disconnect(m_projectConnections[project]);
        m_projectConnections.remove(project);
    }

    LOG_MESSAGE(QString("Removed ignore patterns for project: %1").arg(project->displayName()));
}

void IgnoreManager::reloadAllPatterns()
{
    QList<ProjectExplorer::Project *> projects = m_projectIgnorePatterns.keys();

    for (ProjectExplorer::Project *project : projects) {
        if (project) {
            reloadIgnorePatterns(project);
        }
    }

    m_ignoreCache.clear();
}

QString IgnoreManager::ignoreFilePath(ProjectExplorer::Project *project) const
{
    if (!project) {
        return QString();
    }

    return project->projectDirectory().toUrlishString() + "/.qodeassistignore";
}

} // namespace QodeAssist::Context
