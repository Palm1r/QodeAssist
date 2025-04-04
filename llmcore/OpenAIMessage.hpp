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

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace QodeAssist::LLMCore {

class OpenAIMessage
{
public:
    struct Choice
    {
        QString content;
        QString finishReason;
    };

    struct Usage
    {
        int promptTokens{0};
        int completionTokens{0};
        int totalTokens{0};
    };

    Choice choice;
    QString error;
    bool done{false};
    Usage usage;

    QString getContent() const;
    bool hasError() const;
    bool isDone() const;

    static OpenAIMessage fromJson(const QJsonObject &obj);
};

} // namespace QodeAssist::LLMCore
