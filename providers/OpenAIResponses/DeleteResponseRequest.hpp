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

namespace QodeAssist::OpenAIResponses {

struct DeleteResponseRequest
{
    QString responseId;

    QString buildUrl(const QString &baseUrl) const
    {
        return QString("%1/v1/responses/%2").arg(baseUrl, responseId);
    }

    bool isValid() const { return !responseId.isEmpty(); }
};

class DeleteResponseRequestBuilder
{
public:
    DeleteResponseRequestBuilder &setResponseId(const QString &id)
    {
        m_request.responseId = id;
        return *this;
    }

    DeleteResponseRequest build() const { return m_request; }

private:
    DeleteResponseRequest m_request;
};

struct DeleteResponseResult
{
    bool success = false;
    QString message;

    static DeleteResponseResult fromJson(const QJsonObject &obj)
    {
        DeleteResponseResult result;
        result.success = obj["success"].toBool();
        result.message = obj["message"].toString();
        return result;
    }
};

} // namespace QodeAssist::OpenAIResponses

