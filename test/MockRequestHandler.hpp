/*
 * Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
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

#include <llmcore/RequestHandlerBase.hpp>

namespace QodeAssist::LLMCore {

class MockRequestHandler : public RequestHandlerBase
{
public:
    explicit MockRequestHandler(QObject *parent = nullptr)
        : RequestHandlerBase(parent)
        , m_fakeCompletion("")
    {}

    void setFakeCompletion(const QString &completion) { m_fakeCompletion = completion; }

    void sendLLMRequest(const LLMConfig &config, const QJsonObject &request) override
    {
        m_receivedRequests.append(config);

        emit completionReceived(m_fakeCompletion, request, true);

        QString requestId = request["id"].toString();
        emit requestFinished(requestId, true, QString());
    }

    bool cancelRequest(const QString &id) override
    {
        emit requestCancelled(id);
        return true;
    }

    const QVector<LLMConfig> &receivedRequests() const { return m_receivedRequests; }

private:
    QString m_fakeCompletion;
    QVector<LLMConfig> m_receivedRequests;
};

} // namespace QodeAssist::LLMCore
