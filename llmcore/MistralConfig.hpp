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
#include <QStringList>

namespace QodeAssist::LLMCore {

class MistralConfig
{
    QJsonObject m_data;

public:
    MistralConfig &maxTokens(int v) { m_data["max_tokens"] = v; return *this; }
    MistralConfig &temperature(double v) { m_data["temperature"] = v; return *this; }
    MistralConfig &stream(bool v) { m_data["stream"] = v; return *this; }
    MistralConfig &topP(double v) { m_data["top_p"] = v; return *this; }
    MistralConfig &topK(int v) { m_data["top_k"] = v; return *this; }
    MistralConfig &frequencyPenalty(double v) { m_data["frequency_penalty"] = v; return *this; }
    MistralConfig &presencePenalty(double v) { m_data["presence_penalty"] = v; return *this; }
    MistralConfig &stop(const QStringList &v)
    {
        m_data["stop"] = QJsonArray::fromStringList(v);
        return *this;
    }

    const QJsonObject &json() const { return m_data; }
    operator const QJsonObject &() const { return m_data; }
};

} // namespace QodeAssist::LLMCore
