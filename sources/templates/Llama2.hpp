// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "templates/PromptTemplate.hpp"
#include <QJsonArray>

namespace QodeAssist::Templates {

class Llama2 : public Templates::PromptTemplate
{
public:
    QString name() const override { return "Llama 2"; }
    Templates::TemplateType type() const override { return Templates::TemplateType::Chat; }
    QStringList stopWords() const override { return QStringList() << "[INST]"; }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        QString fullContent;

        if (context.systemPrompt) {
            fullContent
                += QString("[INST]<<SYS>>\n%1\n<</SYS>>[/INST]\n").arg(context.systemPrompt.value());
        }

        if (context.history) {
            for (const auto &msg : context.history.value()) {
                if (msg.role == "user") {
                    fullContent += QString("[INST]%1[/INST]\n").arg(msg.content);
                } else if (msg.role == "assistant") {
                    fullContent += msg.content + "\n";
                }
            }
        }

        messages.append(QJsonObject{{"role", "user"}, {"content", fullContent}});

        request["messages"] = messages;
    }
    QString description() const override
    {
        return "Template for Llama 2 models:\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\n"
               "      \"role\": \"user\",\n"
               "      \"content\": \"[INST]<<SYS>>\\n<system prompt>\\n<</SYS>>[/INST]\\n"
               "<assistant response>\\n"
               "[INST]<user message>[/INST]\\n\"\n"
               "    }\n"
               "  ]\n"
               "}\n\n"
               "Compatible with Ollama, LM Studio, and other services for Llama 2.";
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
