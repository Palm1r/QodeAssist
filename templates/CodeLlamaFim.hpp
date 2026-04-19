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

#include "pluginllmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class CodeLlamaFim : public PluginLLMCore::PromptTemplate
{
public:
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::FIM; }
    QString name() const override { return "CodeLlama FIM"; }
    QString endpoint() const override { return QStringLiteral("/api/generate"); }
    QStringList stopWords() const override
    {
        return QStringList() << "<EOT>" << "<PRE>" << "<SUF" << "<MID>";
    }
    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
    {
        request["prompt"] = QString("<PRE> %1 <SUF>%2 <MID>")
                                .arg(context.prefix.value_or(""), context.suffix.value_or(""));
        request["system"] = context.systemPrompt.value_or("");
    }
    QString description() const override
    {
        return "Specialized template for CodeLlama FIM:\n\n"
               "{\n"
               "  \"prompt\": \"<PRE> <code prefix> <SUF><code suffix> <MID>\",\n"
               "  \"system\": \"<system prompt>\"\n"
               "}\n\n"
               "Optimized for code completion with CodeLlama models.";
    }
    bool isSupportProvider(PluginLLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::PluginLLMCore::ProviderID::Ollama:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
