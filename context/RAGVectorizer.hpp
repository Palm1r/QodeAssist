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

#include <QFuture>
#include <QNetworkAccessManager>
#include <QObject>

#include <RAGData.hpp>

namespace QodeAssist::Context {

class RAGVectorizer : public QObject
{
    Q_OBJECT
public:
    explicit RAGVectorizer(
        const QString &providerUrl = "http://localhost:11434",
        const QString &modelName = "all-minilm:33m-l12-v2-fp16",
        QObject *parent = nullptr);
    ~RAGVectorizer();

    QFuture<RAGVector> vectorizeText(const QString &text);

private:
    QJsonObject prepareEmbeddingRequest(const QString &text) const;
    RAGVector parseEmbeddingResponse(const QByteArray &response) const;

    QNetworkAccessManager *m_network;
    QString m_embedProviderUrl;
    QString m_model;
};

} // namespace QodeAssist::Context
