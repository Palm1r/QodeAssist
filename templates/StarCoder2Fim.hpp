// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pluginllmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class StarCoder2Fim : public PluginLLMCore::PromptTemplate
{
public:
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::FIM; }
    QString name() const override { return "StarCoder2 FIM"; }
    QString endpoint() const override { return QStringLiteral("/api/generate"); }
    QStringList stopWords() const override
    {
        return QStringList() << "<|endoftext|>" << "<file_sep>" << "<fim_prefix>" << "<fim_suffix>"
                             << "<fim_middle>";
    }
    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
    {
        request["prompt"] = QString("<fim_prefix>%1<fim_suffix>%2<fim_middle>")
                                .arg(context.prefix.value_or(""), context.suffix.value_or(""));
        request["system"] = context.systemPrompt.value_or("");
    }
    QString description() const override
    {
        return "Template for StarCoder2 with FIM format:\n\n"
               "{\n"
               "  \"prompt\": \"<fim_prefix><code prefix><fim_suffix><code suffix><fim_middle>\",\n"
               "  \"system\": \"<system prompt>\"\n"
               "}\n\n"
               "Includes stop words to prevent token duplication.";
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
