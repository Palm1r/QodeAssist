// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>

#include "pluginllmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class Llama3 : public PluginLLMCore::PromptTemplate
{
public:
    QString name() const override { return "Llama 3"; }
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::Chat; }
    QStringList stopWords() const override
    {
        return QStringList() << "<|start_header_id|>" << "<|end_header_id|>" << "<|eot_id|>";
    }
    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        if (context.systemPrompt) {
            messages.append(QJsonObject{
                {"role", "system"},
                {"content",
                 QString("<|start_header_id|>system<|end_header_id|>%2<|eot_id|>")
                     .arg(context.systemPrompt.value())}});
        }

        if (context.history) {
            for (const auto &msg : context.history.value()) {
                messages.append(QJsonObject{
                    {"role", msg.role},
                    {"content",
                     QString("<|start_header_id|>%1<|end_header_id|>%2<|eot_id|>")
                         .arg(msg.role, msg.content)}});
            }
        }

        request["messages"] = messages;
    }
    QString description() const override
    {
        return "Template for Llama 3 models:\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\n"
               "      \"role\": \"system\",\n"
               "      \"content\": \"<|start_header_id|>system<|end_header_id|><system "
               "prompt><|eot_id|>\"\n"
               "    },\n"
               "    {\n"
               "      \"role\": \"user\",\n"
               "      \"content\": \"<|start_header_id|>user<|end_header_id|><user "
               "message><|eot_id|>\"\n"
               "    }\n"
               "  ]\n"
               "}\n\n"
               "Compatible with Ollama, LM Studio, and OpenAI-compatible services for Llama 3.";
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
