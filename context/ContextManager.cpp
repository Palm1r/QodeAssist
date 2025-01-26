/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include "ContextManager.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <projectexplorer/project.h>
#include <projectexplorer/projectnodes.h>

namespace QodeAssist::Context {

ContextManager &ContextManager::instance()
{
    static ContextManager manager;
    return manager;
}

ContextManager::ContextManager(QObject *parent)
    : QObject(parent)
{}

QString ContextManager::readFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream in(&file);
    return in.readAll();
}

QList<ContentFile> ContextManager::getContentFiles(const QStringList &filePaths) const
{
    QList<ContentFile> files;
    for (const QString &path : filePaths) {
        ContentFile contentFile = createContentFile(path);
        files.append(contentFile);
    }
    return files;
}

ContentFile ContextManager::createContentFile(const QString &filePath) const
{
    ContentFile contentFile;
    QFileInfo fileInfo(filePath);
    contentFile.filename = fileInfo.fileName();
    contentFile.content = readFile(filePath);
    return contentFile;
}

bool ContextManager::isInBuildDirectory(const QString &filePath) const
{
    static const QStringList buildDirPatterns
        = {"/build/",
           "/Build/",
           "/BUILD/",
           "/debug/",
           "/Debug/",
           "/DEBUG/",
           "/release/",
           "/Release/",
           "/RELEASE/",
           "/builds/"};

    for (const QString &pattern : buildDirPatterns) {
        if (filePath.contains(pattern)) {
            return true;
        }
    }
    return false;
}

QStringList ContextManager::getProjectSourceFiles(ProjectExplorer::Project *project) const
{
    QStringList sourceFiles;
    if (!project)
        return sourceFiles;

    auto projectNode = project->rootProjectNode();
    if (!projectNode)
        return sourceFiles;

    projectNode->forEachNode(
        [&sourceFiles, this](ProjectExplorer::FileNode *fileNode) {
            if (fileNode) {
                QString filePath = fileNode->filePath().toString();
                if (shouldProcessFile(filePath) && !isInBuildDirectory(filePath)) {
                    sourceFiles.append(filePath);
                }
            }
        },
        nullptr);

    return sourceFiles;
}

bool ContextManager::shouldProcessFile(const QString &filePath) const
{
    static const QStringList supportedExtensions
        = {"cpp", "hpp", "c", "h", "cc", "hh", "cxx", "hxx", "qml", "js", "py"};

    QFileInfo fileInfo(filePath);
    return supportedExtensions.contains(fileInfo.suffix().toLower());
}

} // namespace QodeAssist::Context
