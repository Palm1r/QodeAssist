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

class CodeLlamaFim : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::FIM; }
    QString name() const override { return "CodeLlama FIM"; }
    QStringList stopWords() const override
    {
        return QStringList() << "<EOT>" << "<PRE>" << "<SUF" << "<MID>";
    }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        request["prompt"] = QString("<PRE> %1 <SUF>%2 <MID>")
                                .arg(context.prefix.value_or(""), context.suffix.value_or(""));
        request["system"] = context.systemPrompt.value_or("");
    }
    QString description() const override
    {
        return "The message will contain the following tokens: <PRE> %1 <SUF>%2 <MID>";
    }
};

} // namespace QodeAssist::Templates
