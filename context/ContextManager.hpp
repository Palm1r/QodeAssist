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

#pragma once

#include "ContentFile.hpp"

#include <QObject>
#include <QString>

#include "FileChunker.hpp"

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::Context {

class ContextManager : public QObject
{
    Q_OBJECT

public:
    static ContextManager &instance();

    QString readFile(const QString &filePath) const;
    QList<ContentFile> getContentFiles(const QStringList &filePaths) const;
    QStringList getProjectSourceFiles(ProjectExplorer::Project *project) const;

    void testProjectChunks(
        ProjectExplorer::Project *project, const FileChunker::ChunkingConfig &config);

private:
    explicit ContextManager(QObject *parent = nullptr);
    ~ContextManager() = default;
    ContextManager(const ContextManager &) = delete;
    ContextManager &operator=(const ContextManager &) = delete;

    ContentFile createContentFile(const QString &filePath) const;
    bool shouldProcessFile(const QString &filePath) const;
    bool isInBuildDirectory(const QString &filePath) const;
};

} // namespace QodeAssist::Context
