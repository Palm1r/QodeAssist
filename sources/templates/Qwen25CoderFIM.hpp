// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "templates/PromptTemplate.hpp"
#include <QJsonArray>

namespace QodeAssist::Templates {

class Qwen25CoderFIM : public Templates::PromptTemplate
{
public:
    QString name() const override { return "Qwen2.5 Coder FIM"; }
    Templates::TemplateType type() const override { return Templates::TemplateType::FIM; }
    QString endpoint() const override { return QStringLiteral("/api/generate"); }
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
