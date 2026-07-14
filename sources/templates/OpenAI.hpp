// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonArray>

#include "templates/PromptTemplate.hpp"
#include "templates/ToolMessages.hpp"

namespace QodeAssist::Templates {

class OpenAI : public Templates::PromptTemplate
{
public:
    Templates::TemplateType type() const override { return Templates::TemplateType::Chat; }
    QString name() const override { return "OpenAI"; }
    QStringList stopWords() const override { return QStringList(); }
    bool supportsToolHistory() const override { return true; }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        if (context.systemPrompt) {
            messages.append(
                QJsonObject{{"role", "system"}, {"content", context.systemPrompt.value()}});
        }

        if (context.history) {
            for (const auto &msg : context.history.value()) {
                if (appendOpenAIToolMessage(messages, msg)) {
                    continue;
                }
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
        return "Template for OpenAI models (GPT series):\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\"role\": \"system\", \"content\": \"<system prompt>\"},\n"
               "    {\"role\": \"user\", \"content\": \"<user message>\"},\n"
               "    {\"role\": \"assistant\", \"content\": \"<assistant response>\"}\n"
               "  ]\n"
               "}\n\n"
               "Standard Chat API format for OpenAI.";
    }
    bool isSupportProvider(Providers::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::Providers::ProviderID::OpenAI:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
