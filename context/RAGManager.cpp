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

#include "RAGManager.hpp"
#include "EnhancedRAGSimilaritySearch.hpp"
#include "RAGPreprocessor.hpp"
#include "RAGSimilaritySearch.hpp"
#include "logger/Logger.hpp"

#include <coreplugin/icore.h>
#include <projectexplorer/project.h>
#include <QFile>
#include <QtConcurrent>

namespace QodeAssist::Context {

RAGManager &RAGManager::instance()
{
    static RAGManager manager;
    return manager;
}

RAGManager::RAGManager(QObject *parent)
    : QObject(parent)
    , m_vectorizer(std::make_unique<RAGVectorizer>())
{}

RAGManager::~RAGManager() {}

QString RAGManager::getStoragePath(ProjectExplorer::Project *project) const
{
    return QString("%1/qodeassist/%2/rag/vectors.db")
        .arg(Core::ICore::userResourcePath().toString(), project->displayName());
}

std::optional<QString> RAGManager::loadFileContent(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "ERROR: Failed to open file for reading:" << filePath
                 << "Error:" << file.errorString();
        return std::nullopt;
    }

    QFileInfo fileInfo(filePath);
    qDebug() << "Loading content from file:" << fileInfo.fileName() << "Size:" << fileInfo.size()
             << "bytes";

    QString content = QString::fromUtf8(file.readAll());
    if (content.isEmpty()) {
        qDebug() << "WARNING: Empty content read from file:" << filePath;
    }
    return content;
}

void RAGManager::ensureStorageForProject(ProjectExplorer::Project *project) const
{
    qDebug() << "Ensuring storage for project:" << project->displayName();

    if (m_currentProject == project && m_currentStorage) {
        qDebug() << "Using existing storage";
        return;
    }

    qDebug() << "Creating new storage";
    m_currentStorage.reset();
    m_currentProject = project;

    if (project) {
        QString storagePath = getStoragePath(project);
        qDebug() << "Storage path:" << storagePath;

        StorageOptions options;
        m_currentStorage = std::make_unique<RAGStorage>(storagePath, options);

        qDebug() << "Initializing storage...";
        if (!m_currentStorage->init()) {
            qDebug() << "Failed to initialize storage";
            m_currentStorage.reset();
            return;
        }
        qDebug() << "Storage initialized successfully";
    }
}

QFuture<void> RAGManager::processProjectFiles(
    ProjectExplorer::Project *project,
    const QStringList &filePaths,
    const FileChunker::ChunkingConfig &config)
{
    qDebug() << "\nStarting batch processing of" << filePaths.size()
             << "files for project:" << project->displayName();

    auto promise = std::make_shared<QPromise<void>>();
    promise->start();

    qDebug() << "Initializing storage...";
    ensureStorageForProject(project);

    if (!m_currentStorage) {
        qDebug() << "Failed to initialize storage for project:" << project->displayName();
        promise->finish();
        return promise->future();
    }
    qDebug() << "Storage initialized successfully";

    qDebug() << "Checking files for processing...";
    QSet<QString> uniqueFiles;
    for (const QString &filePath : filePaths) {
        qDebug() << "Checking file:" << filePath;
        if (isFileStorageOutdated(project, filePath)) {
            qDebug() << "File needs processing:" << filePath;
            uniqueFiles.insert(filePath);
        }
    }

    QStringList filesToProcess = uniqueFiles.values();

    if (filesToProcess.isEmpty()) {
        qDebug() << "No files need processing";
        emit vectorizationFinished();
        promise->finish();
        return promise->future();
    }

    qDebug() << "Starting to process" << filesToProcess.size() << "files";
    const int batchSize = 10;
    processNextFileBatch(promise, project, filesToProcess, config, 0, batchSize);

    return promise->future();
}

void RAGManager::processNextFileBatch(
    std::shared_ptr<QPromise<void>> promise,
    ProjectExplorer::Project *project,
    const QStringList &files,
    const FileChunker::ChunkingConfig &config,
    int startIndex,
    int batchSize)
{
    if (startIndex >= files.size()) {
        qDebug() << "All batches processed successfully";
        emit vectorizationFinished();
        promise->finish();
        return;
    }

    int endIndex = qMin(startIndex + batchSize, files.size());
    auto currentBatch = files.mid(startIndex, endIndex - startIndex);

    qDebug() << "\nProcessing batch" << (startIndex / batchSize + 1) << "(" << currentBatch.size()
             << "files)"
             << "\nProgress:" << startIndex << "to" << endIndex << "of" << files.size();

    for (const QString &filePath : currentBatch) {
        qDebug() << "Starting processing file:" << filePath;
        auto future = processFileWithChunks(project, filePath, config);
        auto watcher = new QFutureWatcher<bool>;
        watcher->setFuture(future);

        connect(
            watcher,
            &QFutureWatcher<bool>::finished,
            this,
            [this,
             watcher,
             promise,
             project,
             files,
             startIndex,
             endIndex,
             batchSize,
             config,
             filePath]() {
                bool success = watcher->result();
                qDebug() << "File processed:" << filePath << "success:" << success;

                bool isLastFileInBatch = (filePath == files[endIndex - 1]);
                if (isLastFileInBatch) {
                    qDebug() << "Batch completed, moving to next batch";
                    emit vectorizationProgress(endIndex, files.size());
                    processNextFileBatch(promise, project, files, config, endIndex, batchSize);
                }

                watcher->deleteLater();
            });
    }
}

QFuture<bool> RAGManager::processFileWithChunks(
    ProjectExplorer::Project *project,
    const QString &filePath,
    const FileChunker::ChunkingConfig &config)
{
    auto promise = std::make_shared<QPromise<bool>>();
    promise->start();

    ensureStorageForProject(project);
    if (!m_currentStorage) {
        qDebug() << "Storage not initialized for file:" << filePath;
        promise->addResult(false);
        promise->finish();
        return promise->future();
    }

    auto fileContent = loadFileContent(filePath);
    if (!fileContent) {
        qDebug() << "Failed to load content for file:" << filePath;
        promise->addResult(false);
        promise->finish();
        return promise->future();
    }

    qDebug() << "Creating chunks for file:" << filePath;
    auto chunksFuture = m_chunker.chunkFiles({filePath});
    auto chunks = chunksFuture.result();

    if (chunks.isEmpty()) {
        qDebug() << "No chunks created for file:" << filePath;
        promise->addResult(false);
        promise->finish();
        return promise->future();
    }

    qDebug() << "Created" << chunks.size() << "chunks for file:" << filePath;

    // Преобразуем FileChunk в FileChunkData
    QList<FileChunkData> chunkData;
    for (const auto &chunk : chunks) {
        FileChunkData data;
        data.filePath = chunk.filePath;
        data.startLine = chunk.startLine;
        data.endLine = chunk.endLine;
        data.content = chunk.content;
        chunkData.append(data);
    }

    qDebug() << "Deleting old chunks for file:" << filePath;
    m_currentStorage->deleteChunksForFile(filePath);

    auto vectorizeFuture = vectorizeAndStoreChunks(filePath, chunkData);
    auto watcher = new QFutureWatcher<void>;
    watcher->setFuture(vectorizeFuture);

    connect(watcher, &QFutureWatcher<void>::finished, this, [promise, watcher, filePath]() {
        qDebug() << "Completed processing file:" << filePath;
        promise->addResult(true);
        promise->finish();
        watcher->deleteLater();
    });

    return promise->future();
}

QFuture<void> RAGManager::vectorizeAndStoreChunks(
    const QString &filePath, const QList<FileChunkData> &chunks)
{
    qDebug() << "Vectorizing and storing" << chunks.size() << "chunks for file:" << filePath;

    auto promise = std::make_shared<QPromise<void>>();
    promise->start();

    // Обрабатываем чанки последовательно
    processNextChunk(promise, chunks, 0);

    return promise->future();
}

void RAGManager::processNextChunk(
    std::shared_ptr<QPromise<void>> promise, const QList<FileChunkData> &chunks, int currentIndex)
{
    if (currentIndex >= chunks.size()) {
        promise->finish();
        return;
    }

    const auto &chunk = chunks[currentIndex];
    QString processedContent = RAGPreprocessor::preprocessCode(chunk.content);
    qDebug() << "Processing chunk" << currentIndex + 1 << "of" << chunks.size();

    auto vectorFuture = m_vectorizer->vectorizeText(processedContent);
    auto watcher = new QFutureWatcher<RAGVector>;
    watcher->setFuture(vectorFuture);

    connect(
        watcher,
        &QFutureWatcher<RAGVector>::finished,
        this,
        [this, watcher, promise, chunks, currentIndex, chunk]() {
            auto vector = watcher->result();

            if (!vector.empty()) {
                qDebug() << "Storing vector and chunk for file:" << chunk.filePath;
                bool vectorStored = m_currentStorage->storeVector(chunk.filePath, vector);
                bool chunkStored = m_currentStorage->storeChunk(chunk);
                qDebug() << "Storage results - Vector:" << vectorStored << "Chunk:" << chunkStored;
            } else {
                qDebug() << "Failed to vectorize chunk content";
            }

            processNextChunk(promise, chunks, currentIndex + 1);

            watcher->deleteLater();
        });
}

QFuture<QList<RAGManager::ChunkSearchResult>> RAGManager::findRelevantChunks(
    const QString &query, ProjectExplorer::Project *project, int topK)
{
    auto promise = std::make_shared<QPromise<QList<ChunkSearchResult>>>();
    promise->start();

    ensureStorageForProject(project);
    if (!m_currentStorage) {
        qDebug() << "Storage not initialized for project:" << project->displayName();
        promise->addResult({});
        promise->finish();
        return promise->future();
    }

    QString processedQuery = RAGPreprocessor::preprocessCode(query);

    auto vectorFuture = m_vectorizer->vectorizeText(processedQuery);
    vectorFuture.then([this, promise, project, processedQuery, topK](const RAGVector &queryVector) {
        if (queryVector.empty()) {
            qDebug() << "Failed to vectorize query";
            promise->addResult({});
            promise->finish();
            return;
        }

        auto files = m_currentStorage->getFilesWithChunks();
        QList<FileChunkData> allChunks;

        for (const auto &filePath : files) {
            auto fileChunks = m_currentStorage->getChunksForFile(filePath);
            allChunks.append(fileChunks);
        }

        auto results = rankChunks(queryVector, processedQuery, allChunks);

        if (results.size() > topK) {
            results = results.mid(0, topK);
        }

        qDebug() << "Found" << results.size() << "relevant chunks";
        promise->addResult(results);
        promise->finish();

        closeStorage();
    });

    return promise->future();
}

QList<RAGManager::ChunkSearchResult> RAGManager::rankChunks(
    const RAGVector &queryVector, const QString &queryText, const QList<FileChunkData> &chunks)
{
    QList<ChunkSearchResult> results;
    results.reserve(chunks.size());

    for (const auto &chunk : chunks) {
        auto chunkVector = m_currentStorage->getVector(chunk.filePath);
        if (!chunkVector.has_value()) {
            continue;
        }

        QString processedChunk = RAGPreprocessor::preprocessCode(chunk.content);

        auto similarity = EnhancedRAGSimilaritySearch::calculateSimilarity(
            queryVector, chunkVector.value(), queryText, processedChunk);

        results.append(ChunkSearchResult{
            chunk.filePath,
            chunk.startLine,
            chunk.endLine,
            chunk.content,
            similarity.semantic_similarity,
            similarity.structural_similarity,
            similarity.combined_score});
    }

    std::sort(results.begin(), results.end());

    return results;
}

QStringList RAGManager::getStoredFiles(ProjectExplorer::Project *project) const
{
    ensureStorageForProject(project);
    if (!m_currentStorage) {
        return {};
    }
    return m_currentStorage->getAllFiles();
}

bool RAGManager::isFileStorageOutdated(
    ProjectExplorer::Project *project, const QString &filePath) const
{
    ensureStorageForProject(project);
    if (!m_currentStorage) {
        return true;
    }
    return m_currentStorage->needsUpdate(filePath);
}

std::optional<RAGVector> RAGManager::loadVectorFromStorage(
    ProjectExplorer::Project *project, const QString &filePath)
{
    ensureStorageForProject(project);
    if (!m_currentStorage) {
        return std::nullopt;
    }
    return m_currentStorage->getVector(filePath);
}

void RAGManager::closeStorage()
{
    qDebug() << "Closing storage...";
    if (m_currentStorage) {
        m_currentStorage.reset();
        m_currentProject = nullptr;
        qDebug() << "Storage closed";
    }
}

} // namespace QodeAssist::Context
