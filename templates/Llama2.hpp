// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pluginllmcore/PromptTemplate.hpp"
#include <QJsonArray>

namespace QodeAssist::Templates {

class Llama2 : public PluginLLMCore::PromptTemplate
{
public:
    QString name() const override { return "Llama 2"; }
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::Chat; }
    QStringList stopWords() const override { return QStringList() << "[INST]"; }
    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
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
    bool isSupportProvider(PluginLLMCore::ProviderID id) const override
    {
        switch (id) {
        case PluginLLMCore::ProviderID::Ollama:
        case PluginLLMCore::ProviderID::LMStudio:
        case PluginLLMCore::ProviderID::OpenRouter:
        case PluginLLMCore::ProviderID::OpenAICompatible:
        case PluginLLMCore::ProviderID::LlamaCpp:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
