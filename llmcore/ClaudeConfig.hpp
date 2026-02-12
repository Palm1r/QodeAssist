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

#include <optional>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

namespace QodeAssist::LLMCore {

class ClaudeConfig
{
    QJsonObject m_data;

public:
    ClaudeConfig &maxTokens(int v) { m_data["max_tokens"] = v; return *this; }
    ClaudeConfig &temperature(double v) { m_data["temperature"] = v; return *this; }
    ClaudeConfig &stream(bool v) { m_data["stream"] = v; return *this; }
    ClaudeConfig &topP(double v) { m_data["top_p"] = v; return *this; }
    ClaudeConfig &topK(int v) { m_data["top_k"] = v; return *this; }
    ClaudeConfig &stopSequences(const QStringList &v)
    {
        m_data["stop_sequences"] = QJsonArray::fromStringList(v);
        return *this;
    }
    ClaudeConfig &thinking(const QString &type, std::optional<int> budgetTokens = {})
    {
        QJsonObject obj;
        obj["type"] = type;
        if (budgetTokens)
            obj["budget_tokens"] = *budgetTokens;
        m_data["thinking"] = obj;
        return *this;
    }

    const QJsonObject &json() const { return m_data; }
    operator const QJsonObject &() const { return m_data; }
};

} // namespace QodeAssist::LLMCore
