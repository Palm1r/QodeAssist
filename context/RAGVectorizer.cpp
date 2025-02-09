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

#include "RAGVectorizer.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace QodeAssist::Context {

RAGVectorizer::RAGVectorizer(const QString &providerUrl,
                             const QString &modelName,
                             QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_embedProviderUrl(providerUrl)
    , m_model(modelName)
{}

RAGVectorizer::~RAGVectorizer() {}

QJsonObject RAGVectorizer::prepareEmbeddingRequest(const QString &text) const
{
    return QJsonObject{{"model", m_model}, {"prompt", text}};
}

RAGVector RAGVectorizer::parseEmbeddingResponse(const QByteArray &response) const
{
    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (doc.isNull()) {
        qDebug() << "Failed to parse JSON response";
        return RAGVector();
    }

    QJsonObject obj = doc.object();
    if (!obj.contains("embedding")) {
        qDebug() << "Response does not contain 'embedding' field";
        // qDebug() << "Response content:" << response;
        return RAGVector();
    }

    QJsonArray array = obj["embedding"].toArray();
    if (array.isEmpty()) {
        qDebug() << "Embedding array is empty";
        return RAGVector();
    }

    RAGVector result;
    result.reserve(array.size());
    for (const auto &value : array) {
        result.push_back(value.toDouble());
    }

    qDebug() << "Successfully parsed vector with size:" << result.size();
    return result;
}

QFuture<RAGVector> RAGVectorizer::vectorizeText(const QString &text)
{
    qDebug() << "Vectorizing text, length:" << text.length();
    qDebug() << "Using embedding provider:" << m_embedProviderUrl;

    auto promise = std::make_shared<QPromise<RAGVector>>();
    promise->start();

    QNetworkRequest request(QUrl(m_embedProviderUrl + "/api/embeddings"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject requestData = prepareEmbeddingRequest(text);
    QByteArray jsonData = QJsonDocument(requestData).toJson();
    qDebug() << "Sending request to embeddings API:" << jsonData;

    auto reply = m_network->post(request, jsonData);

    connect(reply, &QNetworkReply::finished, this, [promise, reply, this]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            // qDebug() << "Received response from embeddings API:" << response;

            auto vector = parseEmbeddingResponse(response);
            qDebug() << "Parsed vector size:" << vector.size();
            promise->addResult(vector);
        } else {
            qDebug() << "Network error:" << reply->errorString();
            qDebug() << "HTTP status code:"
                     << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            qDebug() << "Response:" << reply->readAll();

            promise->addResult(RAGVector());
        }
        promise->finish();
        reply->deleteLater();
    });

    return promise->future();
}

} // namespace QodeAssist::Context
