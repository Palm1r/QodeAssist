#include "EmbeddingsGenerator.h"

#include <QJsonDocument>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QtNetwork/qnetworkreply.h>
#include "Logger.hpp"

namespace QodeAssist::LLMCore {

EmbeddingsGenerator &EmbeddingsGenerator::instance()
{
    static EmbeddingsGenerator instance;
    return instance;
}

void EmbeddingsGenerator::generateEmbeddings(const QVector<CodeChunk> &chunks)
{
    qDebug() << "Starting embeddings generation for" << chunks.size() << "chunks";

    m_pendingChunks = chunks;
    if (!m_isProcessing) {
        processNextChunk();
    }
}

EmbeddingsGenerator::EmbeddingsGenerator(QObject *parent)
    : QObject(parent)
{
    // Настраиваем обработку SSL-ошибок, если нужно
    m_networkManager.setTransferTimeout(30000); // 30 секунд таймаут
}

void EmbeddingsGenerator::generateEmbedding(const QString &message)
{
    QJsonObject requestData = prepareMessageRequest(message);

    QUrl url(QString("%1%2").arg(BASE_URL, ENDPOINT));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    qDebug() << "Sending request for message embedding, message length:" << message.length();

    QByteArray jsonData = QJsonDocument(requestData).toJson();
    QNetworkReply *reply = m_networkManager.post(request, jsonData);

    // Сохраняем исходное сообщение в reply
    reply->setProperty("message", message);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleMessageResponse(reply);
        reply->deleteLater();
    });
}

void EmbeddingsGenerator::processNextChunk()
{
    if (m_pendingChunks.isEmpty()) {
        m_isProcessing = false;
        qDebug() << "Finished processing all chunks";
        return;
    }

    m_isProcessing = true;
    auto chunk = m_pendingChunks.takeFirst();

    // Формируем содержимое с учетом перекрытия
    QString content = chunk.content;
    if (!chunk.overlapContent.isEmpty()) {
        content += chunk.overlapContent;
    }

    QJsonObject requestData = prepareRequest(content);

    QUrl url(QString("%1%2").arg(BASE_URL, ENDPOINT));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    qDebug() << "Sending request for chunk"
             << "lines:" << chunk.startLine << "-" << chunk.endLine
             << "content size:" << content.length();

    QByteArray jsonData = QJsonDocument(requestData).toJson();
    QNetworkReply *reply = m_networkManager.post(request, jsonData);

    // Сохраняем чанк в reply, чтобы использовать в обработчике
    reply->setProperty("chunk", QVariant::fromValue(chunk));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleResponse(reply);
        reply->deleteLater();
    });
}

QJsonObject EmbeddingsGenerator::prepareRequest(const QString &content) const
{
    QJsonObject request;
    request["model"] = MODEL_NAME;
    request["prompt"] = content;

    // Дополнительные параметры модели, если нужны
    QJsonObject options;
    options["temperature"] = 0.0; // Для эмбеддингов обычно используется 0
    options["top_p"] = 1.0;
    request["options"] = options;

    return request;
}

QJsonObject EmbeddingsGenerator::prepareMessageRequest(const QString &message) const
{
    QJsonObject request;
    request["model"] = MODEL_NAME;
    // Форматируем сообщение через шаблон для улучшения совместимости с кодовыми эмбеддингами
    request["prompt"] = QString(QUERY_TEMPLATE).arg(message);

    QJsonObject options;
    options["temperature"] = 0.0;
    options["top_p"] = 1.0;
    request["options"] = options;

    return request;
}

void EmbeddingsGenerator::handleResponse(QNetworkReply *reply)
{
    auto chunk = reply->property("chunk").value<CodeChunk>();

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("Network error for chunk %1-%2: %3")
                               .arg(chunk.startLine)
                               .arg(chunk.endLine)
                               .arg(reply->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
    } else {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);

        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("embedding")) {
                QJsonArray embeddingArray = obj["embedding"].toArray();
                QVector<float> embedding;
                embedding.reserve(embeddingArray.size());

                for (const auto &value : embeddingArray) {
                    embedding.append(value.toDouble());
                }

                qDebug() << "Generated embedding for chunk"
                         << "lines:" << chunk.startLine << "-" << chunk.endLine
                         << "embedding size:" << embedding.size();

                emit embeddingGenerated(chunk, embedding);
            } else {
                emit error("No embedding in response");
            }
        } else {
            emit error("Invalid JSON response");
        }
    }

    // Обрабатываем следующий чанк
    processNextChunk();
}

void EmbeddingsGenerator::handleMessageResponse(QNetworkReply *reply)
{
    QString message = reply->property("message").toString();

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("Network error for message embedding: %1")
                               .arg(reply->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("embedding")) {
            QJsonArray embeddingArray = obj["embedding"].toArray();
            QVector<float> embedding;
            embedding.reserve(embeddingArray.size());

            for (const auto &value : embeddingArray) {
                embedding.append(value.toDouble());
            }

            qDebug() << "Generated embedding for message"
                     << "embedding size:" << embedding.size();

            emit messageEmbeddingGenerated(message, embedding);
        } else {
            emit error("No embedding in response");
        }
    } else {
        emit error("Invalid JSON response");
    }
}

} // namespace QodeAssist::LLMCore
