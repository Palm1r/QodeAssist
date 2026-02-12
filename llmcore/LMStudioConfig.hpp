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

#include <QJsonObject>

namespace QodeAssist::LLMCore {

class LMStudioConfig
{
    QJsonObject m_data;

public:
    LMStudioConfig &maxTokens(int v) { m_data["max_tokens"] = v; return *this; }
    LMStudioConfig &temperature(double v) { m_data["temperature"] = v; return *this; }
    LMStudioConfig &topP(double v) { m_data["top_p"] = v; return *this; }
    LMStudioConfig &topK(int v) { m_data["top_k"] = v; return *this; }
    LMStudioConfig &frequencyPenalty(double v) { m_data["frequency_penalty"] = v; return *this; }
    LMStudioConfig &presencePenalty(double v) { m_data["presence_penalty"] = v; return *this; }

    const QJsonObject &json() const { return m_data; }
    operator const QJsonObject &() const { return m_data; }
};

} // namespace QodeAssist::LLMCore
