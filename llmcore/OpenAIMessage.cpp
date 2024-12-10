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

#include "OpenAIMessage.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QodeAssist::LLMCore {

OpenAIMessage OpenAIMessage::fromJson(const QByteArray &data)
{
    OpenAIMessage msg;

    QByteArrayList lines = data.split('\n');
    QByteArray jsonData;

    for (const QByteArray &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }

        if (line.trimmed() == "data: [DONE]") {
            msg.done = true;
            continue;
        }

        if (line.startsWith("data: ")) {
            jsonData = line.mid(6);
            break;
        }
    }

    if (jsonData.isEmpty()) {
        jsonData = data;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
    if (doc.isNull()) {
        msg.error = QString("Invalid JSON response: %1").arg(error.errorString());
        return msg;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("error")) {
        msg.error = obj["error"].toObject()["message"].toString();
        return msg;
    }

    if (obj.contains("choices")) {
        auto choices = obj["choices"].toArray();
        if (!choices.isEmpty()) {
            auto choiceObj = choices[0].toObject();

            if (choiceObj.contains("message")) {
                msg.choice.content = choiceObj["message"].toObject()["content"].toString();
            } else if (choiceObj.contains("delta")) {
                msg.choice.content = choiceObj["delta"].toObject()["content"].toString();
            }

            msg.choice.finishReason = choiceObj["finish_reason"].toString();
            if (!msg.choice.finishReason.isEmpty()) {
                msg.done = true;
            }
        }
    }

    if (obj.contains("usage")) {
        QJsonObject usage = obj["usage"].toObject();
        msg.usage.promptTokens = usage["prompt_tokens"].toInt();
        msg.usage.completionTokens = usage["completion_tokens"].toInt();
        msg.usage.totalTokens = usage["total_tokens"].toInt();
    }

    return msg;
}

QString OpenAIMessage::getContent() const
{
    return choice.content;
}

bool OpenAIMessage::hasError() const
{
    return !error.isEmpty();
}

bool OpenAIMessage::isDone() const
{
    return done
           || (!choice.finishReason.isEmpty()
               && (choice.finishReason == "stop" || choice.finishReason == "length"));
}

} // namespace QodeAssist::LLMCore
