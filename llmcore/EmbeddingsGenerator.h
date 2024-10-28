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

    void generateEmbeddings(const QVector<CodeChunk> &chunks);
    void generateEmbedding(const QString &message);

signals:
    void embeddingGenerated(const CodeChunk &chunk, const QVector<float> &embedding);
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
    //"starcoder2:15b-instruct"; //"starcoder2:15b"; //"all-minilm"; //"starcoder2:7b";
    static constexpr const char *BASE_URL = "http://localhost:11434";
    static constexpr const char *ENDPOINT = "/api/embeddings";
    static constexpr const char *QUERY_TEMPLATE = "Convert this question into code context: %1";

    QNetworkAccessManager m_networkManager;
    QVector<CodeChunk> m_pendingChunks;
    bool m_isProcessing = false;
};

} // namespace QodeAssist::LLMCore
