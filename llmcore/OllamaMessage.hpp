/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include <QDateTime>
#include <QJsonObject>
#include <QObject>

namespace QodeAssist::LLMCore {

class OllamaMessage
{
public:
    enum class Type { Generate, Chat };

    struct Metrics
    {
        qint64 totalDuration{0};
        qint64 loadDuration{0};
        qint64 promptEvalCount{0};
        qint64 promptEvalDuration{0};
        qint64 evalCount{0};
        qint64 evalDuration{0};
    };

    struct GenerateResponse
    {
        QString response;
        QVector<int> context;
    };

    struct ChatResponse
    {
        QString role;
        QString content;
    };

    QString model;
    QDateTime createdAt;
    std::variant<GenerateResponse, ChatResponse> response;
    bool done{false};
    QString doneReason;
    QString error;
    Metrics metrics;

    static OllamaMessage fromJson(const QByteArray &data, Type type);
    QString getContent() const;
    bool hasError() const;

private:
    static QJsonObject parseJsonFromData(const QByteArray &data);
};

} // namespace QodeAssist::LLMCore
