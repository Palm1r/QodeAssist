/* 
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "llmcore/PromptTemplate.hpp"
#include "providers/OpenAIResponsesRequestBuilder.hpp"

namespace QodeAssist::Templates {

class OpenAIResponses : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const noexcept override 
    { 
        return LLMCore::TemplateType::Chat; 
    }
    
    QString name() const override { return "OpenAI Responses"; }
    
    QStringList stopWords() const override { return {}; }
    
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        using namespace QodeAssist::OpenAIResponses;
        RequestBuilder builder;

        if (context.systemPrompt) {
            builder.setInstructions(context.systemPrompt.value());
        }

        if (!context.history || context.history->isEmpty()) {
            return;
        }

        const auto &history = context.history.value();

        for (const auto &msg : history) {
            if (msg.role == "system") {
                continue;
            }

            Message message;
            message.role = roleFromString(msg.role);

            if (msg.images && !msg.images->isEmpty()) {
                const auto &images = msg.images.value();
                message.content.reserve(1 + images.size());
                
                if (!msg.content.isEmpty()) {
                    message.content.append(MessageContent(InputText{msg.content}));
                }

                for (const auto &image : images) {
                    InputImage imgInput;
                    imgInput.detail = "auto";

                    if (image.isUrl) {
                        imgInput.imageUrl = image.data;
                    } else {
                        imgInput.imageUrl
                            = QString("data:%1;base64,%2").arg(image.mediaType, image.data);
                    }

                    message.content.append(MessageContent(std::move(imgInput)));
                }
            } else {
                message.content.append(MessageContent(msg.content));
            }

            builder.addMessage(std::move(message));
        }

        const QJsonObject builtRequest = builder.toJson();
        for (auto it = builtRequest.constBegin(); it != builtRequest.constEnd(); ++it) {
            request[it.key()] = it.value();
        }
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
               "}\n\n"
               "Uses type-safe RequestBuilder for OpenAI Responses API.";
    }
    bool isSupportProvider(LLMCore::ProviderID id) const noexcept override
    {
        return id == QodeAssist::LLMCore::ProviderID::OpenAIResponses;
    }

private:
    static QodeAssist::OpenAIResponses::Role roleFromString(const QString &roleStr) noexcept
    {
        using namespace QodeAssist::OpenAIResponses;
        
        if (roleStr == "user")
            return Role::User;
        if (roleStr == "assistant")
            return Role::Assistant;
        if (roleStr == "system")
            return Role::System;
        if (roleStr == "developer")
            return Role::Developer;
            
        return Role::User;
    }
};

} // namespace QodeAssist::Templates

