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
#include <QString>
#include <QStringList>

namespace QodeAssist::LLMCore {

class OllamaConfig
{
    QJsonObject m_data;

public:
    OllamaConfig &numPredict(int v) { m_data["num_predict"] = v; return *this; }
    OllamaConfig &temperature(double v) { m_data["temperature"] = v; return *this; }
    OllamaConfig &topP(double v) { m_data["top_p"] = v; return *this; }
    OllamaConfig &topK(int v) { m_data["top_k"] = v; return *this; }
    OllamaConfig &frequencyPenalty(double v) { m_data["frequency_penalty"] = v; return *this; }
    OllamaConfig &presencePenalty(double v) { m_data["presence_penalty"] = v; return *this; }
    OllamaConfig &keepAlive(const QString &v) { m_data["keep_alive"] = v; return *this; }
    OllamaConfig &stop(const QStringList &v)
    {
        m_data["stop"] = QJsonArray::fromStringList(v);
        return *this;
    }
    OllamaConfig &enableThinking(bool v) { m_data["enable_thinking"] = v; return *this; }

    const QJsonObject &json() const { return m_data; }
    operator const QJsonObject &() const { return m_data; }
};

} // namespace QodeAssist::LLMCore
