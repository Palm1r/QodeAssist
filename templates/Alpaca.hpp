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

#include "llmcore/PromptTemplate.hpp"
#include <QJsonArray>

namespace QodeAssist::Templates {

class Alpaca : public LLMCore::PromptTemplate
{
public:
    QString name() const override { return "Alpaca"; }
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Chat; }
    QString promptTemplate() const override { return {}; }
    QStringList stopWords() const override
    {
        return QStringList() << "### Instruction:" << "### Response:";
    }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray messages = request["messages"].toArray();

        for (int i = 0; i < messages.size(); ++i) {
            QJsonObject message = messages[i].toObject();
            QString role = message["role"].toString();
            QString content = message["content"].toString();

            QString formattedContent;
            if (role == "system") {
                formattedContent = content + "\n\n";
            } else if (role == "user") {
                formattedContent = "### Instruction:\n" + content + "\n\n";
            } else if (role == "assistant") {
                formattedContent = "### Response:\n" + content + "\n\n";
            }

            message["content"] = formattedContent;
            messages[i] = message;
        }

        request["messages"] = messages;
    }
    QString description() const override
    {
        return "The message will contain the following tokens: ### Instruction:\n### Response:\n";
    }
};

} // namespace QodeAssist::Templates
