// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>

#include "pluginllmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class ChatML : public PluginLLMCore::PromptTemplate
{
public:
    QString name() const override { return "ChatML"; }
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::Chat; }
    QStringList stopWords() const override
    {
        return QStringList() << "<|im_start|>" << "<|im_end|>";
    }
    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
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
