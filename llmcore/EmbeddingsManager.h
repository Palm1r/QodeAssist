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

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectnodes.h>

#include "CodeChunker.h"
#include "EmbeddingsStorage.hpp"

namespace QodeAssist::LLMCore {

class EmbeddingManager : public QObject
{
    Q_OBJECT

public:
    static EmbeddingManager &instance();

    void processProject(ProjectExplorer::Project *project);
    QString getEmbeddingsPath(ProjectExplorer::Project *project) const;
    void findSimilarCode(const QString &query,
                         ProjectExplorer::Project *project,
                         float minSimilarity,
                         int maxResults);

signals:
    void searchCompleted(const QString &query,
                         const QVector<LLMCore::SearchResult> &results,
                         const QString &formattedResponse);
    void searchError(const QString &error);

private:
    EmbeddingManager() = default;
    ~EmbeddingManager() = default;
    EmbeddingManager(const EmbeddingManager &) = delete;
    EmbeddingManager &operator=(const EmbeddingManager &) = delete;

    QVector<CodeChunk> m_allChunks;

    bool ensureEmbeddingsDirectory(ProjectExplorer::Project *project);
    void processDirectory(const QString &path);
    void processFile(const QString &filePath);
};

} // namespace QodeAssist::LLMCore
