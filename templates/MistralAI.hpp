// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>

#include "pluginllmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class MistralAIFim : public PluginLLMCore::PromptTemplate
{
public:
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::FIM; }
    QString name() const override { return "Mistral AI FIM"; }
    QString endpoint() const override { return QStringLiteral("/v1/fim/completions"); }
    QStringList stopWords() const override { return QStringList(); }
    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
    {
        request["prompt"] = context.prefix.value_or("");
        request["suffix"] = context.suffix.value_or("");
    }
    QString description() const override
    {
        return "Template for MistralAI models with FIM support:\n\n"
               "{\n"
               "  \"prompt\": \"<code prefix>\",\n"
               "  \"suffix\": \"<code suffix>\"\n"
               "}\n\n"
               "Optimized for code completion with MistralAI models.";
    }
    bool isSupportProvider(PluginLLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::PluginLLMCore::ProviderID::MistralAI:
            return true;
        default:
            return false;
        }
    }
};

class MistralAIChat : public PluginLLMCore::PromptTemplate
{
public:
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::Chat; }
    QString name() const override { return "Mistral AI Chat"; }
    QStringList stopWords() const override { return QStringList(); }

    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        if (context.systemPrompt) {
            messages.append(
                QJsonObject{{"role", "system"}, {"content", context.systemPrompt.value()}});
        }

        if (context.history) {
            for (const auto &msg : context.history.value()) {
                if (msg.images && !msg.images->isEmpty()) {
                    QJsonArray content;
                    
                    if (!msg.content.isEmpty()) {
                        content.append(QJsonObject{{"type", "text"}, {"text", msg.content}});
                    }
                    
                    for (const auto &image : msg.images.value()) {
                        QJsonObject imageBlock;
                        imageBlock["type"] = "image_url";
                        
                        QJsonObject imageUrl;
                        if (image.isUrl) {
                            imageUrl["url"] = image.data;
                        } else {
                            imageUrl["url"] = QString("data:%1;base64,%2").arg(image.mediaType, image.data);
                        }
                        imageBlock["image_url"] = imageUrl;
                        content.append(imageBlock);
                    }
                    
                    messages.append(QJsonObject{{"role", msg.role}, {"content", content}});
                } else {
                    messages.append(QJsonObject{{"role", msg.role}, {"content", msg.content}});
                }
            }
        }

        request["messages"] = messages;
    }
    QString description() const override
    {
        return "Template for MistralAI chat-capable models:\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\"role\": \"system\", \"content\": \"<system prompt>\"},\n"
               "    {\"role\": \"user\", \"content\": \"<user message>\"},\n"
               "    {\"role\": \"assistant\", \"content\": \"<assistant response>\"}\n"
               "  ]\n"
               "}\n\n"
               "Supports system messages, conversation history, and images.";
    }
    bool isSupportProvider(PluginLLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::PluginLLMCore::ProviderID::MistralAI:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
