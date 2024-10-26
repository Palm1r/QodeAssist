/* 
 * Copyright (C) 2024 Petr Mironychev
 */

#pragma once

#include <QDateTime>
#include <QDir>
#include <QObject>
#include <QVector>

#include "CodeChunker.h"

namespace QodeAssist::LLMCore {

class EmbeddingsStorage : public QObject
{
    Q_OBJECT

public:
    static EmbeddingsStorage &instance();

    struct SearchResult
    {
        QString content;  // Найденный код
        QString filePath; // Путь к файлу
        int startLine;    // Начальная строка
        int endLine;      // Конечная строка
        float similarity; // Степень схожести (0-1)
    };

    // Сохранение эмбеддинга для чанка
    bool storeEmbedding(const CodeChunk &chunk, const QVector<float> &embedding);

    // Поиск похожего кода по эмбеддингу
    QVector<SearchResult> findSimilarCode(const QVector<float> &queryEmbedding,
                                          float minSimilarity = 0.5f,
                                          int maxResults = 5);

    // Проверка актуальности эмбеддингов для файла
    bool isFileUpToDate(const QString &filePath) const;

    // Очистка эмбеддингов для файла
    void clearFileEmbeddings(const QString &filePath);

private:
    explicit EmbeddingsStorage(QObject *parent = nullptr);

    // Генерация имени файла хранения
    QString generateFileName(const CodeChunk &chunk, const QVector<float> &embedding) const;

    // Путь к файлу для хранения
    QString generateStoragePath(const QString &fileName, bool isMetadata) const;

    void ensureDirectoriesExist() const;
    static float cosineDistance(const QVector<float> &a, const QVector<float> &b);

    // Директории для хранения
    QDir m_storageDir;  // Базовая директория
    QDir m_vectorsDir;  // Директория для .vec файлов
    QDir m_metadataDir; // Директория для .json файлов
};

} // namespace QodeAssist::LLMCore
