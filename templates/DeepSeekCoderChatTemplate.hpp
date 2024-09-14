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
#include "PromptTemplate.hpp"

namespace QodeAssist::Templates {

class DeepSeekCoderChatTemplate : public PromptTemplate
{
public:
    QString name() const override { return "DeepSeek Coder Chat"; }
    TemplateType type() const override { return TemplateType::Chat; }

    QString promptTemplate() const override { return "### Instruction:\n%1\n### Response:\n"; }

    QStringList stopWords() const override
    {
        return QStringList() << "### Instruction:" << "### Response:" << "\n\n### " << "<|EOT|>";
    }

    void prepareRequest(QJsonObject &request, const ContextData &context) const override
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
