// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pluginllmcore/PromptTemplate.hpp"

#include <QJsonArray>
#include <QJsonObject>

namespace QodeAssist::Templates {

class OpenAIResponses : public PluginLLMCore::PromptTemplate
{
public:
    PluginLLMCore::TemplateType type() const noexcept override
    {
        return PluginLLMCore::TemplateType::Chat;
    }

    QString name() const override { return "OpenAI Responses"; }

    QStringList stopWords() const override { return {}; }

    void prepareRequest(
        QJsonObject &request, const PluginLLMCore::ContextData &context) const override
    {
        if (context.systemPrompt) {
            request["instructions"] = context.systemPrompt.value();
        }

        if (!context.history || context.history->isEmpty()) {
            return;
        }

        QJsonArray input;
        for (const auto &msg : context.history.value()) {
            if (msg.role == "system") {
                continue;
            }

            QJsonObject message;
            message["role"] = msg.role;

            const bool hasImages = msg.images && !msg.images->isEmpty();

            if (!hasImages) {
                message["content"] = msg.content;
            } else {
                QJsonArray content;
                if (!msg.content.isEmpty()) {
                    content.append(
                        QJsonObject{{"type", "input_text"}, {"text", msg.content}});
                }

                for (const auto &image : msg.images.value()) {
                    QJsonObject imgObj{{"type", "input_image"}, {"detail", "auto"}};
                    if (image.isUrl) {
                        imgObj["image_url"] = image.data;
                    } else {
                        imgObj["image_url"]
                            = QString("data:%1;base64,%2").arg(image.mediaType, image.data);
                    }
                    content.append(imgObj);
                }

                message["content"] = content;
            }

            input.append(message);
        }

        request["input"] = input;
    }

    QString description() const override
    {
        return "Template for OpenAI Responses API:\n\n"
               "Simple request:\n"
               "{\n"
               "  \"input\": \"<user message>\"\n"
               "}\n\n"
               "Multi-turn conversation:\n"
               "{\n"
               "  \"instructions\": \"<system prompt>\",\n"
               "  \"input\": [\n"
               "    {\"role\": \"user\", \"content\": \"<message>\"}\n"
               "  ]\n"
               "}";
    }

    bool isSupportProvider(PluginLLMCore::ProviderID id) const noexcept override
    {
        return id == QodeAssist::PluginLLMCore::ProviderID::OpenAIResponses;
    }
};

} // namespace QodeAssist::Templates
