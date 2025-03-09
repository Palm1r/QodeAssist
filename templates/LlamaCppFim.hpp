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

class LlamaCppFim : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::FIM; }
    QString name() const override { return "llama.cpp FIM"; }
    QStringList stopWords() const override { return {}; }

    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        request["input_prefix"] = context.prefix.value_or("");
        request["input_suffix"] = context.suffix.value_or("");
    }

    QString description() const override
    {
        return "Default llama.cpp FIM (Fill-in-Middle) /infill template with native format:\n\n"
               "{\n"
               "  \"input_prefix\": \"<code prefix>\",\n"
               "  \"input_suffix\": \"<code suffix>\",\n"
               "  \"input_extra\": \"<system prompt>\"\n"
               "}\n\n"
               "Recommended for models with FIM capability.";
    }

    bool isSupportProvider(LLMCore::ProviderID id) const override
    {
        return id == QodeAssist::LLMCore::ProviderID::LlamaCpp;
    }
};

} // namespace QodeAssist::Templates
