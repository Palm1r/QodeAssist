/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

class Qwen3CoderFIM : public LLMCore::PromptTemplate
{
public:
    QString name() const override { return "Qwen3 Coder FIM"; }
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Chat; }
    QStringList stopWords() const override { return QStringList() << "<|im_end|>"; }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        messages.append(
            QJsonObject{{"role", "system"}, {"content", context.systemPrompt.value_or("")}});

        messages.append(
            QJsonObject{
                {"role", "user"},
                {"content",
                 QString("<|fim_prefix|>%1<|fim_suffix|>%2<|fim_middle|>")
                     .arg(context.prefix.value_or(""), context.suffix.value_or(""))}});
        request["messages"] = messages;
    }
    QString description() const override
    {
        return "Template for supporting Qwen3 Coder FIM format via chat template:\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\n"
               "      \"role\": \"system\",\n"
               "      \"content\": \"You are a code completion assistant.\"\n"
               "    },\n"
               "    {\n"
               "      \"role\": \"user\",\n"
               "      \"content\": \"<|fim_prefix|>code<|fim_suffix|>code<|fim_middle|>\"\n"
               "    }\n"
               "  ]\n"
               "}\n\n";
    }
    bool isSupportProvider(LLMCore::ProviderID id) const override
    {
        switch (id) {
        case LLMCore::ProviderID::Ollama:
        case LLMCore::ProviderID::LMStudio:
        case LLMCore::ProviderID::OpenRouter:
        case LLMCore::ProviderID::OpenAICompatible:
        case LLMCore::ProviderID::LlamaCpp:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
