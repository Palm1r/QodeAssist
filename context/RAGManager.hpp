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
#include <QFuture>
#include <QObject>
#include <QString>

#include "RAGStorage.hpp"
#include "RAGVectorizer.hpp"
#include <RAGData.hpp>

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::Context {

class RAGManager : public QObject
{
    Q_OBJECT
public:
    static RAGManager &instance();

    struct SearchResult
    {
        QString filePath;
        float l2Score;
        float cosineScore;

        bool operator<(const SearchResult &other) const;
    };

    // Process and vectorize files
    QFuture<void> processFiles(ProjectExplorer::Project *project, const QStringList &filePaths);
    std::optional<RAGVector> loadVectorFromStorage(
        ProjectExplorer::Project *project, const QString &filePath);
    QStringList getStoredFiles(ProjectExplorer::Project *project) const;
    bool isFileStorageOutdated(ProjectExplorer::Project *project, const QString &filePath) const;
    RAGVectorizer *getVectorizer() const { return m_vectorizer.get(); }

    // Search functionality
    QFuture<QList<SearchResult>> search(
        const QString &text, ProjectExplorer::Project *project, int topK = 5);
    void searchSimilarDocuments(const QString &text, ProjectExplorer::Project *project, int topK = 5);
    void logSearchResults(const QList<SearchResult> &results) const;

signals:
    void vectorizationProgress(int processed, int total);
    void vectorizationFinished();

private:
    RAGManager(QObject *parent = nullptr);
    ~RAGManager();
    RAGManager(const RAGManager &) = delete;
    RAGManager &operator=(const RAGManager &) = delete;

    QFuture<bool> processFile(ProjectExplorer::Project *project, const QString &filePath);
    void processNextBatch(
        std::shared_ptr<QPromise<void>> promise,
        ProjectExplorer::Project *project,
        const QStringList &files,
        int startIndex,
        int batchSize);
    void ensureStorageForProject(ProjectExplorer::Project *project);
    QString getStoragePath(ProjectExplorer::Project *project) const;

    std::unique_ptr<RAGVectorizer> m_vectorizer;
    std::unique_ptr<RAGStorage> m_currentStorage;
    ProjectExplorer::Project *m_currentProject{nullptr};
};

} // namespace QodeAssist::Context
