// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

