// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "templates/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class StarCoder2Fim : public Templates::PromptTemplate
{
public:
    Templates::TemplateType type() const override { return Templates::TemplateType::FIM; }
    QString name() const override { return "StarCoder2 FIM"; }
    QString endpoint() const override { return QStringLiteral("/api/generate"); }
    QStringList stopWords() const override
    {
        return QStringList() << "<|endoftext|>" << "<file_sep>" << "<fim_prefix>" << "<fim_suffix>"
                             << "<fim_middle>";
    }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
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
    bool isSupportProvider(Providers::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::Providers::ProviderID::Ollama:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
