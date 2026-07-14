// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "templates/PromptTemplate.hpp"
#include <QJsonArray>

namespace QodeAssist::Templates {

class Alpaca : public Templates::PromptTemplate
{
public:
    QString name() const override { return "Alpaca"; }
    Templates::TemplateType type() const override { return Templates::TemplateType::Chat; }
    QStringList stopWords() const override
    {
        return QStringList() << "### Instruction:" << "### Response:";
    }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        QString fullContent;

        if (context.systemPrompt) {
            fullContent += context.systemPrompt.value() + "\n\n";
        }

        if (context.history) {
            for (const auto &msg : context.history.value()) {
                if (msg.role == "user") {
                    fullContent += QString("### Instruction:\n%1\n\n").arg(msg.content);
                } else if (msg.role == "assistant") {
                    fullContent += QString("### Response:\n%1\n\n").arg(msg.content);
                }
            }
        }

        messages.append(QJsonObject{{"role", "user"}, {"content", fullContent}});

        request["messages"] = messages;
    }
    QString description() const override
    {
        return "Template for models using Alpaca instruction format:\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\n"
               "      \"role\": \"user\",\n"
               "      \"content\": \"<system prompt>\\n\\n"
               "### Instruction:\\n<user message>\\n\\n"
               "### Response:\\n<assistant response>\\n\\n\"\n"
               "    }\n"
               "  ]\n"
               "}\n\n"
               "Combines all messages into a single formatted prompt.";
    }
    bool isSupportProvider(Providers::ProviderID id) const override
    {
        switch (id) {
        case Providers::ProviderID::Ollama:
        case Providers::ProviderID::LMStudio:
        case Providers::ProviderID::OpenRouter:
        case Providers::ProviderID::OpenAICompatible:
        case Providers::ProviderID::LlamaCpp:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
