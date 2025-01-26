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
    QJsonArray array = doc.object()["embedding"].toArray();

    RAGVector result;
    result.reserve(array.size());
    for (const auto &value : array) {
        result.push_back(value.toDouble());
    }
    return result;
}

QFuture<RAGVector> RAGVectorizer::vectorizeText(const QString &text)
{
    auto promise = std::make_shared<QPromise<RAGVector>>();
    promise->start();

    QNetworkRequest request(QUrl(m_embedProviderUrl + "/api/embeddings"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto reply = m_network->post(request, QJsonDocument(prepareEmbeddingRequest(text)).toJson());

    connect(reply, &QNetworkReply::finished, this, [promise, reply, this]() {
        if (reply->error() == QNetworkReply::NoError) {
            promise->addResult(parseEmbeddingResponse(reply->readAll()));
        } else {
            // TODO check error setException
            promise->addResult(RAGVector());
        }
        promise->finish();
        reply->deleteLater();
    });

    return promise->future();
}

} // namespace QodeAssist::Context
