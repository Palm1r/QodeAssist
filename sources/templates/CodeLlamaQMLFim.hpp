// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "templates/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class CodeLlamaQMLFim : public Templates::PromptTemplate
{
public:
    Templates::TemplateType type() const override { return Templates::TemplateType::FIM; }
    QString name() const override { return "CodeLlama QML FIM"; }
    QString endpoint() const override { return QStringLiteral("/api/generate"); }
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
