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

class ClaudeCodeCompletion : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Chat; }
    QString name() const override { return "Claude Code Completion"; }
    QString promptTemplate() const override { return {}; }
    QStringList stopWords() const override { return QStringList(); }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray messages = request["messages"].toArray();

        for (int i = 0; i < messages.size(); ++i) {
            QJsonObject message = messages[i].toObject();
            QString role = message["role"].toString();
            QString content = message["content"].toString();

            if (message["role"] == "user") {
                message["content"] = QString("Complete the code ONLY between these "
                                             "parts:\nBefore: %1\nAfter: %2\n")
                                         .arg(context.prefix, context.suffix);
            } else {
                message["content"] = QString("%1").arg(content);
            }

            messages[i] = message;
        }

        request["messages"] = messages;
    }
    QString description() const override { return "Claude Chat for code completion"; }
};

class ClaudeChat : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Chat; }
    QString name() const override { return "Claude Chat"; }
    QString promptTemplate() const override { return {}; }
    QStringList stopWords() const override { return QStringList(); }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override {}
    QString description() const override { return "Claude Chat"; }
};

} // namespace QodeAssist::Templates
