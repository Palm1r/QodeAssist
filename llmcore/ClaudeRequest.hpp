/*
 * Copyright (C) 2026 Petr Mironychev
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
#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include "ProviderID.hpp"

namespace QodeAssist::LLMCore {

class ClaudeRequest
{
public:
    virtual ~ClaudeRequest() = default;

    virtual QString endpoint() const = 0;
    virtual QJsonObject toJson() const { return {}; }
    virtual bool isPost() const { return true; }

    // API metadata
    static constexpr auto Name = "Claude";
    static constexpr auto ApiVersion = "2023-06-01";
    static constexpr auto DefaultBaseUrl = "https://api.anthropic.com";
    static ProviderID providerID() { return ProviderID::Claude; }

    // Endpoints
    static QString modelsEndpoint() { return "/v1/models"; }
    static QString messagesEndpoint() { return "/v1/messages"; }

    static void prepareNetworkRequest(QNetworkRequest &request, const QString &apiKey)
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("anthropic-version", ApiVersion);
        if (!apiKey.isEmpty()) {
            request.setRawHeader("x-api-key", apiKey.toUtf8());
        }
    }

    QNetworkRequest createNetworkRequest(const QString &baseUrl, const QString &apiKey) const
    {
        QNetworkRequest request(QUrl(baseUrl + endpoint()));
        prepareNetworkRequest(request, apiKey);
        return request;
    }
};

class ClaudeListModelsRequest : public ClaudeRequest
{
public:
    QString endpoint() const override { return modelsEndpoint(); }
    bool isPost() const override { return false; }

    QNetworkRequest createNetworkRequest(const QString &baseUrl, const QString &apiKey) const
    {
        QUrl url(baseUrl + endpoint());
        QUrlQuery query;
        query.addQueryItem("limit", "1000");
        url.setQuery(query);

        QNetworkRequest request(url);
        prepareNetworkRequest(request, apiKey);
        return request;
    }

    static QList<QString> parseResponse(const QJsonObject &response)
    {
        QList<QString> models;
        if (response.contains("data")) {
            const QJsonArray modelArray = response["data"].toArray();
            for (const QJsonValue &value : modelArray) {
                QJsonObject modelObject = value.toObject();
                if (modelObject.contains("id")) {
                    models.append(modelObject["id"].toString());
                }
            }
        }
        return models;
    }
};

class ClaudeChatRequest : public ClaudeRequest
{
    QJsonObject m_payload;

public:
    explicit ClaudeChatRequest(const QJsonObject &payload)
        : m_payload(payload)
    {}

    QString endpoint() const override { return messagesEndpoint(); }
    QJsonObject toJson() const override { return m_payload; }
};

} // namespace QodeAssist::LLMCore
