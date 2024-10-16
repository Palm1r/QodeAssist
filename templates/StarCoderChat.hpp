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

#include <QJsonArray>
#include "llmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class StarCoderChat : public LLMCore::PromptTemplate
{
public:
    QString name() const override { return "StarCoder Chat"; }
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Chat; }
    QString promptTemplate() const override { return "### Instruction:\n%1\n### Response:\n"; }
    QStringList stopWords() const override
    {
        return QStringList() << "###"
                             << "<|endoftext|>" << "<file_sep>";
    }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QString formattedPrompt = promptTemplate().arg(context.prefix);
        QJsonArray messages = request["messages"].toArray();

        QJsonObject newMessage;
        newMessage["role"] = "user";
        newMessage["content"] = formattedPrompt;
        messages.append(newMessage);

        request["messages"] = messages;
    }
};
} // namespace QodeAssist::Templates
