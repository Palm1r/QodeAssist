// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

