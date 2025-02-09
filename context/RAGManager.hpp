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

#include <memory>
#include <optional>
#include <QFuture>
#include <QObject>

#include "FileChunker.hpp"
#include "RAGData.hpp"
#include "RAGStorage.hpp"
#include "RAGVectorizer.hpp"

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::Context {

class RAGManager : public QObject
{
    Q_OBJECT

public:
    struct ChunkSearchResult
    {
        QString filePath;
        int startLine;
        int endLine;
        QString content;
        float semanticScore;
        float structuralScore;
        float combinedScore;

        bool operator<(const ChunkSearchResult &other) const
        {
            return combinedScore > other.combinedScore;
        }
    };

    static RAGManager &instance();

    QFuture<void> processProjectFiles(
        ProjectExplorer::Project *project,
        const QStringList &filePaths,
        const FileChunker::ChunkingConfig &config = FileChunker::ChunkingConfig());

    QFuture<QList<ChunkSearchResult>> findRelevantChunks(
        const QString &query, ProjectExplorer::Project *project, int topK = 5);

    QStringList getStoredFiles(ProjectExplorer::Project *project) const;
    bool isFileStorageOutdated(ProjectExplorer::Project *project, const QString &filePath) const;

    void processNextChunk(
        std::shared_ptr<QPromise<void>> promise,
        const QList<FileChunkData> &chunks,
        int currentIndex);
    void closeStorage();
signals:
    void vectorizationProgress(int processed, int total);
    void vectorizationFinished();

private:
    explicit RAGManager(QObject *parent = nullptr);
    ~RAGManager();
    RAGManager(const RAGManager &) = delete;
    RAGManager &operator=(const RAGManager &) = delete;

    QString getStoragePath(ProjectExplorer::Project *project) const;
    void ensureStorageForProject(ProjectExplorer::Project *project) const;
    std::optional<QString> loadFileContent(const QString &filePath);
    std::optional<RAGVector> loadVectorFromStorage(
        ProjectExplorer::Project *project, const QString &filePath);

    void processNextFileBatch(
        std::shared_ptr<QPromise<void>> promise,
        ProjectExplorer::Project *project,
        const QStringList &files,
        const FileChunker::ChunkingConfig &config,
        int startIndex,
        int batchSize);

    QFuture<bool> processFileWithChunks(
        ProjectExplorer::Project *project,
        const QString &filePath,
        const FileChunker::ChunkingConfig &config);

    QFuture<void> vectorizeAndStoreChunks(
        const QString &filePath, const QList<FileChunkData> &chunks);

    QList<ChunkSearchResult> rankChunks(
        const RAGVector &queryVector, const QString &queryText, const QList<FileChunkData> &chunks);

private:
    mutable std::unique_ptr<RAGVectorizer> m_vectorizer;
    mutable std::unique_ptr<RAGStorage> m_currentStorage;
    mutable ProjectExplorer::Project *m_currentProject{nullptr};
    FileChunker m_chunker;
};

} // namespace QodeAssist::Context
