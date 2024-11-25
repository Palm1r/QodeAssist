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

namespace QodeAssist::Templates {

class DeepSeekCoderFim : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Fim; }
    QString name() const override { return "DeepSeekCoder FIM"; }
    QString promptTemplate() const override
    {
        return "<｜fim▁begin｜>%1<｜fim▁hole｜>%2<｜fim▁end｜>";
    }
    QStringList stopWords() const override { return QStringList(); }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QString formattedPrompt = promptTemplate().arg(context.prefix, context.suffix);
        request["prompt"] = formattedPrompt;
    }
    QString description() const override
    {
        return "The message will contain the following tokens: "
               "<｜fim▁begin｜>%1<｜fim▁hole｜>%2<｜fim▁end｜>";
    }
};

} // namespace QodeAssist::Templates
