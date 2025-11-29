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

#include <QString>

namespace QodeAssist::OpenAIResponses {

struct CancelResponseRequest
{
    QString responseId;

    QString buildUrl(const QString &baseUrl) const
    {
        return QString("%1/v1/responses/%2/cancel").arg(baseUrl, responseId);
    }

    bool isValid() const { return !responseId.isEmpty(); }
};

class CancelResponseRequestBuilder
{
public:
    CancelResponseRequestBuilder &setResponseId(const QString &id)
    {
        m_request.responseId = id;
        return *this;
    }

    CancelResponseRequest build() const { return m_request; }

private:
    CancelResponseRequest m_request;
};

} // namespace QodeAssist::OpenAIResponses

