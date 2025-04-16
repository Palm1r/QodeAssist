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
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        for (auto it = m_projectIgnorePatterns.begin(); it != m_projectIgnorePatterns.end(); ++it) {
            if (ignoreFilePath(it.key()) == path) {
                reloadIgnorePatterns(it.key());
                break;
            }
        }
    });
}

IgnoreManager::~IgnoreManager() = default;

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

void IgnoreManager::reloadIgnorePatterns(ProjectExplorer::Project *project)
{
    if (!project)
        return;

    QStringList patterns = loadIgnorePatterns(project);
    m_projectIgnorePatterns[project] = patterns;

    QString ignoreFile = ignoreFilePath(project);
    if (QFileInfo::exists(ignoreFile)) {
        if (m_fileWatcher.files().contains(ignoreFile))
            m_fileWatcher.removePath(ignoreFile);

        m_fileWatcher.addPath(ignoreFile);

        if (!m_projectConnections.contains(project)) {
            auto connection = connect(project, &QObject::destroyed, this, [this, project]() {
                m_projectIgnorePatterns.remove(project);
                m_projectConnections.remove(project);
            });
            m_projectConnections[project] = connection;
        }
    }
}

QStringList IgnoreManager::loadIgnorePatterns(ProjectExplorer::Project *project)
{
    QStringList patterns;
    if (!project)
        return patterns;

    QString path = ignoreFilePath(project);
    QFile file(path);

    if (!file.exists())
        return patterns;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("Could not open .qodeassistignore file: %1").arg(path));
        return patterns;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith('#'))
            patterns << line;
    }

    return patterns;
}

bool IgnoreManager::matchesIgnorePatterns(const QString &path, const QStringList &patterns) const
{
    for (const QString &pattern : patterns) {
        QString regexPattern = QRegularExpression::escape(pattern);

        regexPattern.replace("\\*\\*", ".*"); // ** matches any characters including /
        regexPattern.replace("\\*", "[^/]*"); // * matches any characters except /
        regexPattern.replace("\\?", ".");     // ? matches any single character

        regexPattern = QString("^%1$").arg(regexPattern);

        QRegularExpression regex(regexPattern);
        if (regex.match(path).hasMatch())
            return true;
    }

    return false;
}

QString IgnoreManager::ignoreFilePath(ProjectExplorer::Project *project) const
{
    if (!project)
        return QString();

    return project->projectDirectory().toUrlishString() + "/.qodeassist/qodeassistignore";
}

} // namespace QodeAssist::Context
