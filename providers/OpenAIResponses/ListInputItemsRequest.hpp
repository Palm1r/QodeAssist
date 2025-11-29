/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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
#include <QString>
#include <QStringList>
#include <optional>

namespace QodeAssist::OpenAIResponses {

enum class SortOrder { Ascending, Descending };

struct ListInputItemsRequest
{
    QString responseId;
    std::optional<QString> after;
    std::optional<QStringList> include;
    std::optional<int> limit;
    std::optional<SortOrder> order;

    QString buildUrl(const QString &baseUrl) const
    {
        QString url = QString("%1/v1/responses/%2/input_items").arg(baseUrl, responseId);
        QStringList queryParams;

        if (after) {
            queryParams.append(QString("after=%1").arg(*after));
        }

        if (include && !include->isEmpty()) {
            for (const auto &item : *include) {
                queryParams.append(QString("include=%1").arg(item));
            }
        }

        if (limit) {
            queryParams.append(QString("limit=%1").arg(*limit));
        }

        if (order) {
            QString orderStr = (*order == SortOrder::Ascending) ? "asc" : "desc";
            queryParams.append(QString("order=%1").arg(orderStr));
        }

        if (!queryParams.isEmpty()) {
            url += "?" + queryParams.join("&");
        }

        return url;
    }

    bool isValid() const
    {
        if (responseId.isEmpty()) {
            return false;
        }

        if (limit && (*limit < 1 || *limit > 100)) {
            return false;
        }

        return true;
    }
};

class ListInputItemsRequestBuilder
{
public:
    ListInputItemsRequestBuilder &setResponseId(const QString &id)
    {
        m_request.responseId = id;
        return *this;
    }

    ListInputItemsRequestBuilder &setAfter(const QString &itemId)
    {
        m_request.after = itemId;
        return *this;
    }

    ListInputItemsRequestBuilder &setInclude(const QStringList &include)
    {
        m_request.include = include;
        return *this;
    }

    ListInputItemsRequestBuilder &addInclude(const QString &item)
    {
        if (!m_request.include) {
            m_request.include = QStringList();
        }
        m_request.include->append(item);
        return *this;
    }

    ListInputItemsRequestBuilder &setLimit(int limit)
    {
        m_request.limit = limit;
        return *this;
    }

    ListInputItemsRequestBuilder &setOrder(SortOrder order)
    {
        m_request.order = order;
        return *this;
    }

    ListInputItemsRequestBuilder &setAscendingOrder()
    {
        m_request.order = SortOrder::Ascending;
        return *this;
    }

    ListInputItemsRequestBuilder &setDescendingOrder()
    {
        m_request.order = SortOrder::Descending;
        return *this;
    }

    ListInputItemsRequest build() const { return m_request; }

private:
    ListInputItemsRequest m_request;
};

struct ListInputItemsResponse
{
    QJsonArray data;
    QString firstId;
    QString lastId;
    bool hasMore = false;
    QString object;

    static ListInputItemsResponse fromJson(const QJsonObject &obj)
    {
        ListInputItemsResponse result;
        result.data = obj["data"].toArray();
        result.firstId = obj["first_id"].toString();
        result.lastId = obj["last_id"].toString();
        result.hasMore = obj["has_more"].toBool();
        result.object = obj["object"].toString();
        return result;
    }
};

} // namespace QodeAssist::OpenAIResponses

