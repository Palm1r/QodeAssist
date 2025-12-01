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

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <optional>

namespace QodeAssist::OpenAIResponses {

struct GetResponseRequest
{
    QString responseId;
    std::optional<QStringList> include;
    std::optional<bool> includeObfuscation;
    std::optional<int> startingAfter;
    std::optional<bool> stream;

    QString buildUrl(const QString &baseUrl) const
    {
        QString url = QString("%1/v1/responses/%2").arg(baseUrl, responseId);
        QStringList queryParams;

        if (include && !include->isEmpty()) {
            for (const auto &item : *include) {
                queryParams.append(QString("include=%1").arg(item));
            }
        }

        if (includeObfuscation) {
            queryParams.append(
                QString("include_obfuscation=%1").arg(*includeObfuscation ? "true" : "false"));
        }

        if (startingAfter) {
            queryParams.append(QString("starting_after=%1").arg(*startingAfter));
        }

        if (stream) {
            queryParams.append(QString("stream=%1").arg(*stream ? "true" : "false"));
        }

        if (!queryParams.isEmpty()) {
            url += "?" + queryParams.join("&");
        }

        return url;
    }

    bool isValid() const { return !responseId.isEmpty(); }
};

class GetResponseRequestBuilder
{
public:
    GetResponseRequestBuilder &setResponseId(const QString &id)
    {
        m_request.responseId = id;
        return *this;
    }

    GetResponseRequestBuilder &setInclude(const QStringList &include)
    {
        m_request.include = include;
        return *this;
    }

    GetResponseRequestBuilder &addInclude(const QString &item)
    {
        if (!m_request.include) {
            m_request.include = QStringList();
        }
        m_request.include->append(item);
        return *this;
    }

    GetResponseRequestBuilder &setIncludeObfuscation(bool enabled)
    {
        m_request.includeObfuscation = enabled;
        return *this;
    }

    GetResponseRequestBuilder &setStartingAfter(int sequence)
    {
        m_request.startingAfter = sequence;
        return *this;
    }

    GetResponseRequestBuilder &setStream(bool enabled)
    {
        m_request.stream = enabled;
        return *this;
    }

    GetResponseRequest build() const { return m_request; }

private:
    GetResponseRequest m_request;
};

} // namespace QodeAssist::OpenAIResponses

