// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>

#include "pluginllmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class OpenAICompatible : public PluginLLMCore::PromptTemplate
{
public:
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::Chat; }
    QString name() const override { return "OpenAI Compatible"; }
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
        return "Generic template for OpenAI API-compatible services:\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\"role\": \"system\", \"content\": \"<system prompt>\"},\n"
               "    {\"role\": \"user\", \"content\": \"<user message>\"},\n"
               "    {\"role\": \"assistant\", \"content\": \"<assistant response>\"}\n"
               "  ]\n"
               "}\n\n"
               "Works with any service implementing the OpenAI Chat API specification.\n"
               "Supports images.";
    }
    bool isSupportProvider(PluginLLMCore::ProviderID id) const override
    {
        switch (id) {
        case PluginLLMCore::ProviderID::OpenAICompatible:
        case PluginLLMCore::ProviderID::OpenRouter:
        case PluginLLMCore::ProviderID::LMStudio:
        case PluginLLMCore::ProviderID::LlamaCpp:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
