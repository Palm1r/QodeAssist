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

void RAGManager::ensureStorageForProject(ProjectExplorer::Project *project)
{
    if (m_currentProject == project && m_currentStorage) {
        return;
    }

    m_currentStorage.reset();
    m_currentProject = project;

    if (project) {
        m_currentStorage = std::make_unique<RAGStorage>(getStoragePath(project), this);
        m_currentStorage->init();
    }
}

QFuture<void> RAGManager::processFiles(
    ProjectExplorer::Project *project, const QStringList &filePaths)
{
    qDebug() << "Starting batch processing of" << filePaths.size()
             << "files for project:" << project->displayName();

    auto promise = std::make_shared<QPromise<void>>();
    promise->start();

    ensureStorageForProject(project);
    if (!m_currentStorage) {
        qDebug() << "Failed to initialize storage for project:" << project->displayName();
        promise->finish();
        return promise->future();
    }

    const int batchSize = 10;

    QStringList filesToProcess;
    for (const QString &filePath : filePaths) {
        if (isFileStorageOutdated(project, filePath)) {
            qDebug() << "File needs processing:" << filePath;
            filesToProcess.append(filePath);
        }
    }

    if (filesToProcess.isEmpty()) {
        qDebug() << "No files need processing";
        emit vectorizationFinished();
        promise->finish();
        return promise->future();
    }

    qDebug() << "Processing" << filesToProcess.size() << "files in batches of" << batchSize;

    processNextBatch(promise, project, filesToProcess, 0, batchSize);

    return promise->future();
}

void RAGManager::processNextBatch(
    std::shared_ptr<QPromise<void>> promise,
    ProjectExplorer::Project *project,
    const QStringList &files,
    int startIndex,
    int batchSize)
{
    if (startIndex >= files.size()) {
        qDebug() << "All batches processed";
        emit vectorizationFinished();
        promise->finish();
        return;
    }

    int endIndex = qMin(startIndex + batchSize, files.size());
    auto currentBatch = files.mid(startIndex, endIndex - startIndex);

    qDebug() << "Processing batch" << startIndex / batchSize + 1 << "files" << startIndex << "to"
             << endIndex;

    for (const QString &filePath : currentBatch) {
        qDebug() << "Starting processing of file:" << filePath;
        auto future = processFile(project, filePath);
        auto watcher = new QFutureWatcher<bool>;
        watcher->setFuture(future);

        connect(
            watcher,
            &QFutureWatcher<bool>::finished,
            this,
            [this, watcher, promise, project, files, startIndex, endIndex, batchSize, filePath]() {
                bool success = watcher->result();
                qDebug() << "File processed:" << filePath << "success:" << success;

                bool isLastFileInBatch = (filePath == files[endIndex - 1]);
                if (isLastFileInBatch) {
                    qDebug() << "Batch completed, moving to next batch";
                    emit vectorizationProgress(endIndex, files.size());
                    processNextBatch(promise, project, files, endIndex, batchSize);
                }

                watcher->deleteLater();
            });
    }
}

QFuture<bool> RAGManager::processFile(ProjectExplorer::Project *project, const QString &filePath)
{
    auto promise = std::make_shared<QPromise<bool>>();
    promise->start();

    ensureStorageForProject(project);
    if (!m_currentStorage) {
        promise->addResult(false);
        promise->finish();
        return promise->future();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        promise->addResult(false);
        promise->finish();
        return promise->future();
    }

    auto vectorFuture = m_vectorizer->vectorizeText(QString::fromUtf8(file.readAll()));
    vectorFuture.then([promise, filePath, this](const RAGVector &vector) {
        if (vector.empty()) {
            promise->addResult(false);
        } else {
            bool success = m_currentStorage->storeVector(filePath, vector);
            promise->addResult(success);
        }
        promise->finish();
    });

    return promise->future();
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

QStringList RAGManager::getStoredFiles(ProjectExplorer::Project *project) const
{
    if (m_currentProject != project || !m_currentStorage) {
        auto tempStorage = RAGStorage(getStoragePath(project), nullptr);
        if (!tempStorage.init()) {
            return {};
        }
        return tempStorage.getAllFiles();
    }
    return m_currentStorage->getAllFiles();
}

bool RAGManager::isFileStorageOutdated(
    ProjectExplorer::Project *project, const QString &filePath) const
{
    if (m_currentProject != project || !m_currentStorage) {
        auto tempStorage = RAGStorage(getStoragePath(project), nullptr);
        if (!tempStorage.init()) {
            return true;
        }
        return tempStorage.needsUpdate(filePath);
    }
    return m_currentStorage->needsUpdate(filePath);
}

} // namespace QodeAssist::Context
