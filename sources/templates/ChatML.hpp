// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonArray>

#include "templates/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class ChatML : public Templates::PromptTemplate
{
public:
    QString name() const override { return "ChatML"; }
    Templates::TemplateType type() const override { return Templates::TemplateType::Chat; }
    QStringList stopWords() const override
    {
        return QStringList() << "<|im_start|>" << "<|im_end|>";
    }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        if (context.systemPrompt) {
            messages.append(QJsonObject{
                {"role", "system"},
                {"content",
                 QString("<|im_start|>system\n%2\n<|im_end|>").arg(context.systemPrompt.value())}});
        }

        if (context.history) {
            for (const auto &msg : context.history.value()) {
                messages.append(QJsonObject{
                    {"role", msg.role},
                    {"content",
                     QString("<|im_start|>%1\n%2\n<|im_end|>").arg(msg.role, msg.content)}});
            }
        }

        request["messages"] = messages;
    }
    QString description() const override
    {
        return "Template for models supporting ChatML format:\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\n"
               "      \"role\": \"system\",\n"
               "      \"content\": \"<|im_start|>system\\n<system prompt>\\n<|im_end|>\"\n"
               "    },\n"
               "    {\n"
               "      \"role\": \"user\",\n"
               "      \"content\": \"<|im_start|>user\\n<user message>\\n<|im_end|>\"\n"
               "    }\n"
               "  ]\n"
               "}\n\n"
               "Compatible with multiple providers supporting the ChatML token format.";
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
