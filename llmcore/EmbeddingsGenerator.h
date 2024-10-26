/* 
 * Copyright (C) 2024 Petr Mironychev
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QVector>

#include "CodeChunker.h"

namespace QodeAssist::LLMCore {

class EmbeddingsGenerator : public QObject
{
    Q_OBJECT

public:
    static EmbeddingsGenerator &instance();

    // Существующий метод для чанков кода
    void generateEmbeddings(const QVector<CodeChunk> &chunks);

    // Новый метод для пользовательского сообщения
    void generateEmbedding(const QString &message);

signals:
    // Сигнал о готовности эмбеддинга для чанка
    void embeddingGenerated(const CodeChunk &chunk, const QVector<float> &embedding);
    // Новый сигнал для сообщения
    void messageEmbeddingGenerated(const QString &message, const QVector<float> &embedding);
    void error(const QString &message);

private:
    explicit EmbeddingsGenerator(QObject *parent = nullptr);
    void processNextChunk();
    void handleResponse(QNetworkReply *reply);
    void handleMessageResponse(QNetworkReply *reply);
    QJsonObject prepareRequest(const QString &content) const;
    QJsonObject prepareMessageRequest(const QString &message) const;

    static constexpr const char *MODEL_NAME = "starcoder2:7b";
    static constexpr const char *BASE_URL = "http://localhost:11434";
    static constexpr const char *ENDPOINT = "/api/embeddings";
    // Специальный промпт для преобразования вопроса в эмбеддинг, совместимый с кодом
    static constexpr const char *QUERY_TEMPLATE = "Convert this question into code context: %1";

    QNetworkAccessManager m_networkManager;
    QVector<CodeChunk> m_pendingChunks;
    bool m_isProcessing = false;
};

} // namespace QodeAssist::LLMCore
