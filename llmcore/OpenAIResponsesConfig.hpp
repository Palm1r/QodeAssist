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

namespace QodeAssist::LLMCore {

class OpenAIResponsesConfig
{
    QJsonObject m_data;

public:
    OpenAIResponsesConfig &maxOutputTokens(int v) { m_data["max_output_tokens"] = v; return *this; }
    OpenAIResponsesConfig &topP(double v) { m_data["top_p"] = v; return *this; }
    OpenAIResponsesConfig &reasoning(const QString &effort)
    {
        m_data["reasoning"] = QJsonObject{{"effort", effort}};
        return *this;
    }
    OpenAIResponsesConfig &thinkingMaxTokens(int v) { m_data["thinking_max_tokens"] = v; return *this; }

    const QJsonObject &json() const { return m_data; }
    operator const QJsonObject &() const { return m_data; }
};

} // namespace QodeAssist::LLMCore
