// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
