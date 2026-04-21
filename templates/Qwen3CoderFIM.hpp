// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>

#include "pluginllmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class Qwen3CoderFIM : public PluginLLMCore::PromptTemplate
{
public:
    QString name() const override { return "Qwen3 Coder FIM"; }
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::FIMOnChat; }
    QStringList stopWords() const override { return QStringList() << "<|im_end|>"; }
    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        messages.append(
            QJsonObject{{"role", "system"}, {"content", context.systemPrompt.value_or("")}});

        messages.append(
            QJsonObject{
                {"role", "user"},
                {"content",
                 QString("<|fim_prefix|>%1<|fim_suffix|>%2<|fim_middle|>")
                     .arg(context.prefix.value_or(""), context.suffix.value_or(""))}});
        request["messages"] = messages;
    }
    QString description() const override
    {
        return "Template for supporting Qwen3 Coder FIM format via chat template:\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\n"
               "      \"role\": \"system\",\n"
               "      \"content\": \"You are a code completion assistant.\"\n"
               "    },\n"
               "    {\n"
               "      \"role\": \"user\",\n"
               "      \"content\": \"<|fim_prefix|>code<|fim_suffix|>code<|fim_middle|>\"\n"
               "    }\n"
               "  ]\n"
               "}\n\n";
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
