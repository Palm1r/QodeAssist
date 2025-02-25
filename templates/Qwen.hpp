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

class QwenFim : public LLMCore::PromptTemplate
{
public:
    QString name() const override { return "Qwen FIM"; }
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::FIM; }
    QStringList stopWords() const override { return QStringList() << "<|endoftext|>" << "<|EOT|>"; }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        request["prompt"] = QString("<|fim_prefix|>%1<|fim_suffix|>%2<|fim_middle|>")
                                .arg(context.prefix.value_or(""), context.suffix.value_or(""));
        request["system"] = context.systemPrompt.value_or("");
    }
    QString description() const override
    {
        return "Template for Qwen models with FIM support:\n\n"
               "{\n"
               "  \"prompt\": \"<|fim_prefix|><code prefix><|fim_suffix|><code "
               "suffix><|fim_middle|>\",\n"
               "  \"system\": \"<system prompt>\"\n"
               "}\n\n"
               "Ideal for code completion with Qwen models.";
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
