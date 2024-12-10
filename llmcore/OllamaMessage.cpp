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

#include "OllamaMessage.hpp"
#include <QJsonArray>
#include <QJsonDocument>

namespace QodeAssist::LLMCore {

QJsonObject OllamaMessage::parseJsonFromData(const QByteArray &data)
{
    QByteArrayList lines = data.split('\n');
    for (const QByteArray &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(line, &error);
        if (!doc.isNull() && error.error == QJsonParseError::NoError) {
            return doc.object();
        }
    }
    return QJsonObject();
}

OllamaMessage OllamaMessage::fromJson(const QByteArray &data, Type type)
{
    OllamaMessage msg;
    QJsonObject obj = parseJsonFromData(data);

    if (obj.isEmpty()) {
        msg.error = "Invalid JSON response";
        return msg;
    }

    msg.model = obj["model"].toString();
    msg.createdAt = QDateTime::fromString(obj["created_at"].toString(), Qt::ISODate);
    msg.done = obj["done"].toBool();
    msg.doneReason = obj["done_reason"].toString();
    msg.error = obj["error"].toString();

    if (type == Type::Generate) {
        auto &genResponse = msg.response.emplace<GenerateResponse>();
        genResponse.response = obj["response"].toString();
        if (msg.done && obj.contains("context")) {
            const auto array = obj["context"].toArray();
            genResponse.context.reserve(array.size());
            for (const auto &val : array) {
                genResponse.context.append(val.toInt());
            }
        }
    } else {
        auto &chatResponse = msg.response.emplace<ChatResponse>();
        const auto msgObj = obj["message"].toObject();
        chatResponse.role = msgObj["role"].toString();
        chatResponse.content = msgObj["content"].toString();
    }

    if (msg.done) {
        msg.metrics
            = {obj["total_duration"].toVariant().toLongLong(),
               obj["load_duration"].toVariant().toLongLong(),
               obj["prompt_eval_count"].toVariant().toLongLong(),
               obj["prompt_eval_duration"].toVariant().toLongLong(),
               obj["eval_count"].toVariant().toLongLong(),
               obj["eval_duration"].toVariant().toLongLong()};
    }

    return msg;
}

QString OllamaMessage::getContent() const
{
    if (std::holds_alternative<GenerateResponse>(response)) {
        return std::get<GenerateResponse>(response).response;
    }
    return std::get<ChatResponse>(response).content;
}

bool OllamaMessage::hasError() const
{
    return !error.isEmpty();
}

} // namespace QodeAssist::LLMCore
