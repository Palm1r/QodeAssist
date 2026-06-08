// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonArray>

#include "pluginllmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class OllamaFim : public PluginLLMCore::PromptTemplate
{
public:
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::FIM; }
    QString name() const override { return "Ollama FIM"; }
    QString endpoint() const override { return QStringLiteral("/api/generate"); }
    QStringList stopWords() const override { return QStringList() << "<EOT>"; }
    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
    {
        request["prompt"] = context.prefix.value_or("");
        request["suffix"] = context.suffix.value_or("");
        request["system"] = context.systemPrompt.value_or("");
    }
    QString description() const override
    {
        return "Default Ollama FIM (Fill-in-Middle) template with native format:\n\n"
               "{\n"
               "  \"prompt\": \"<code prefix>\",\n"
               "  \"suffix\": \"<code suffix>\",\n"
               "  \"system\": \"<system prompt>\"\n"
               "}\n\n"
               "Recommended for Ollama models with FIM capability.";
    }
    bool isSupportProvider(PluginLLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::PluginLLMCore::ProviderID::Ollama:
            return true;
        default:
            return false;
        }
    }
};

class OllamaChat : public PluginLLMCore::PromptTemplate
{
public:
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::Chat; }
    QString name() const override { return "Ollama Chat"; }
    QStringList stopWords() const override { return QStringList(); }
    bool supportsToolHistory() const override { return true; }

    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        if (context.systemPrompt) {
            messages.append(
                QJsonObject{{"role", "system"}, {"content", context.systemPrompt.value()}});
        }

        if (context.history) {
            for (const auto &msg : context.history.value()) {
                QJsonObject messageObj;
                messageObj["role"] = msg.role;

                if (!msg.toolCalls.isEmpty()) {
                    QJsonArray toolCalls;
                    for (const auto &call : msg.toolCalls) {
                        toolCalls.append(QJsonObject{
                            {"type", "function"},
                            {"function",
                             QJsonObject{{"name", call.name}, {"arguments", call.arguments}}}});
                    }
                    messageObj["tool_calls"] = toolCalls;
                    if (!msg.content.isEmpty()) {
                        messageObj["content"] = msg.content;
                    }
                } else {
                    messageObj["content"] = msg.content;
                    // Ollama correlates a tool result to its originating
                    // call by tool_name; omitting it breaks multi-tool turns.
                    if (msg.role == QLatin1String("tool") && !msg.toolName.isEmpty()) {
                        messageObj["tool_name"] = msg.toolName;
                    }
                }

                if (msg.images && !msg.images->isEmpty()) {
                    QJsonArray images;
                    for (const auto &image : msg.images.value()) {
                        images.append(image.data);
                    }
                    messageObj["images"] = images;
                }
                
                messages.append(messageObj);
            }
        }

        request["messages"] = messages;
    }
    QString description() const override
    {
        return "Template for Ollama Chat with message array format:\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\"role\": \"system\", \"content\": \"<system prompt>\"},\n"
               "    {\"role\": \"user\", \"content\": \"<user message>\", \"images\": [\"<base64>\"]},\n"
               "    {\"role\": \"assistant\", \"content\": \"<assistant response>\"}\n"
               "  ]\n"
               "}\n\n"
               "Recommended for Ollama models with chat capability.\n"
               "Supports images for multimodal models (e.g., llava).";
    }
    bool isSupportProvider(PluginLLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::PluginLLMCore::ProviderID::Ollama:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
