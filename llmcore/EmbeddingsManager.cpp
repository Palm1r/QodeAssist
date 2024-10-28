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

#include "EmbeddingsManager.h"

#include <texteditor/textdocument.h>

#include "CodeChunker.h"
#include "EmbeddingsGenerator.h"
#include "EmbeddingsStorage.hpp"
#include "Logger.hpp"

namespace QodeAssist::LLMCore {

EmbeddingManager &EmbeddingManager::instance()
{
    static EmbeddingManager instance;
    return instance;
}

void EmbeddingManager::processProject(ProjectExplorer::Project *project)
{
    if (!project) {
        LOG_MESSAGE("No project provided to process");
        return;
    }

    LOG_MESSAGE(QString("Processing project: %1").arg(project->displayName()));

    QString embedDirPath = getEmbeddingsPath(project);
    if (!ensureEmbeddingsDirectory(project)) {
        LOG_MESSAGE("Failed to create embeddings directory");
        return;
    }

    // Устанавливаем путь для хранилища
    LLMCore::EmbeddingsStorage::instance().setStoragePath(embedDirPath);

    // Настраиваем обработку результатов до сбора чанков
    auto &generator = LLMCore::EmbeddingsGenerator::instance();
    disconnect(&generator, nullptr, this, nullptr);

    connect(&generator,
            &LLMCore::EmbeddingsGenerator::embeddingGenerated,
            this,
            [](const LLMCore::CodeChunk &chunk, const QVector<float> &embedding) {
                LOG_MESSAGE(QString("Generated embedding for chunk at lines %1-%2")
                                .arg(chunk.startLine)
                                .arg(chunk.endLine));
                LLMCore::EmbeddingsStorage::instance().storeEmbedding(chunk, embedding);
            });

    connect(&generator, &LLMCore::EmbeddingsGenerator::error, this, [](const QString &error) {
        LOG_MESSAGE(QString("Embedding error: %1").arg(error));
    });

    // Собираем все чанки
    m_allChunks.clear();
    QString projectPath = project->projectDirectory().toString();
    processDirectory(projectPath);

    // После сбора всех чанков передаем их в генератор
    if (!m_allChunks.isEmpty()) {
        LOG_MESSAGE(QString("Collected %1 chunks from project, starting embeddings generation")
                        .arg(m_allChunks.size()));
        generator.generateEmbeddings(m_allChunks);
    }
}

QString EmbeddingManager::getEmbeddingsPath(ProjectExplorer::Project *project) const
{
    if (!project)
        return QString();

    QString projectId = project->projectDirectory().fileName();

    // Construct path: <user_resources>/qodeassist/embeddings/<project_id>
    return QString("%1/qodeassist/embeddings/%2")
        .arg(Core::ICore::userResourcePath().toString(), projectId);
}

void EmbeddingManager::findSimilarCode(const QString &query,
                                       ProjectExplorer::Project *project,
                                       float minSimilarity,
                                       int maxResults)
{
    LOG_MESSAGE(QString("Searching similar code for query: %1").arg(query));

    auto &generator = LLMCore::EmbeddingsGenerator::instance();
    auto &storage = LLMCore::EmbeddingsStorage::instance();

    QString embedDirPath = getEmbeddingsPath(project);
    LLMCore::EmbeddingsStorage::instance().setStoragePath(embedDirPath);

    // Отключаем предыдущие соединения
    disconnect(&generator, nullptr, this, nullptr);

    // Подключаем обработчик для сообщения пользователя
    connect(&generator,
            &LLMCore::EmbeddingsGenerator::messageEmbeddingGenerated,
            this,
            [this, query, &storage, minSimilarity, maxResults](const QString &message,
                                                               const QVector<float> &embedding) {
                LOG_MESSAGE(QString("Generated embedding for query: %1...").arg(query.left(50)));

                // Ищем похожий код
                auto results = storage.findSimilarCode(embedding, minSimilarity, maxResults);

                // Формируем результат поиска
                QVector<LLMCore::SearchResult> searchResults;
                QString formattedResponse;

                for (const auto &result : results) {
                    searchResults.append(result);
                    formattedResponse += result.content;
                }

                LOG_MESSAGE(QString("Found %1 similar code fragments").arg(results.size()));

                emit searchCompleted(query, searchResults, formattedResponse);
            });

    // Подключаем обработчик ошибок
    connect(&generator, &LLMCore::EmbeddingsGenerator::error, this, [this](const QString &error) {
        LOG_MESSAGE(QString("Error generating embedding: %1").arg(error));
        emit searchError(error);
    });

    // Генерируем эмбеддинг для запроса
    generator.generateEmbedding(query);
}

bool EmbeddingManager::ensureEmbeddingsDirectory(ProjectExplorer::Project *project)
{
    QString embedDirPath = getEmbeddingsPath(project);
    QDir embedDir(embedDirPath);

    if (!embedDir.exists()) {
        if (!embedDir.mkpath(".")) {
            LOG_MESSAGE(QString("Failed to create embeddings directory: %1").arg(embedDirPath));
            return false;
        }
        LOG_MESSAGE(QString("Created embeddings directory: %1").arg(embedDirPath));
    }

    return true;
}

void EmbeddingManager::processDirectory(const QString &path)
{
    static const QSet<QString> skipDirs
        = {".git", ".svn", "build", "debug", "release", ".qtc_clangd"};

    static const QSet<QString> supportedExtensions
        = {"cpp", "hpp", "h", "c", "cc", "cxx", "qml", "js", "py", "txt", "md", "cmake"};

    QDirIterator it(path,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString currentPath = it.next();
        QFileInfo info(currentPath);

        if (info.isDir()) {
            if (!skipDirs.contains(info.fileName())) {
                processDirectory(currentPath);
            }
        } else {
            QString extension = info.suffix().toLower();
            if (supportedExtensions.contains(extension)) {
                processFile(currentPath);
            }
        }
    }
}

void EmbeddingManager::processFile(const QString &filePath)
{
    LOG_MESSAGE(QString("Processing file: %1").arg(filePath));

    auto document = new TextEditor::TextDocument;
    QString errorString;
    if (auto result = document->open(&errorString,
                                     Utils::FilePath::fromString(filePath),
                                     Utils::FilePath::fromString(filePath));
        result == Core::IDocument::OpenResult::ReadError) {
        LOG_MESSAGE(QString("Failed to load document: %1. Error: %2").arg(filePath, errorString));
        delete document;
        return;
    }

    // Добавляем чанки к общему списку
    m_allChunks.append(LLMCore::CodeChunker::instance().splitDocument(document));
    LOG_MESSAGE(QString("Added %1 chunks from file %2").arg(m_allChunks.size()).arg(filePath));

    delete document;
}

} // namespace QodeAssist::LLMCore
