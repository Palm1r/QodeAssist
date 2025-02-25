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

class CodeLlamaQMLFim : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::FIM; }
    QString name() const override { return "CodeLlama QML FIM"; }
    QStringList stopWords() const override
    {
        return QStringList() << "<SUF>" << "<PRE>" << "</PRE>" << "</SUF>" << "< EOT >" << "\\end"
                             << "<MID>" << "</MID>" << "##";
    }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        request["prompt"] = QString("<SUF>%1<PRE>%2<MID>")
                                .arg(context.suffix.value_or(""), context.prefix.value_or(""));
        request["system"] = context.systemPrompt.value_or("");
    }
    QString description() const override
    {
        return "Specialized template for QML code completion with CodeLlama:\n\n"
               "{\n"
               "  \"prompt\": \"<SUF><code suffix><PRE><code prefix><MID>\",\n"
               "  \"system\": \"<system prompt>\"\n"
               "}\n\n"
               "Specifically optimized for QML/JavaScript code completion.";
    }
    bool isSupportProvider(LLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::LLMCore::ProviderID::Ollama:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
