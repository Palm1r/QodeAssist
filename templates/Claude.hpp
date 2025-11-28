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

#include <QJsonArray>

#include "llmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class Claude : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Chat; }
    QString name() const override { return "Claude"; }
    QStringList stopWords() const override { return QStringList(); }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        if (context.systemPrompt) {
            request["system"] = context.systemPrompt.value();
        }

        if (context.history) {
            for (const auto &msg : context.history.value()) {
                if (msg.role == "system") continue;
                
                if (msg.isThinking) {
                    // Claude API requires signature for thinking blocks
                    if (msg.signature.isEmpty()) {
                        continue;
                    }
                    
                    QJsonArray content;
                    QJsonObject thinkingBlock;
                    thinkingBlock["type"] = msg.isRedacted ? "redacted_thinking" : "thinking";
                    
                    QString thinkingText = msg.content;
                    int signaturePos = thinkingText.indexOf("\n[Signature: ");
                    if (signaturePos != -1) {
                        thinkingText = thinkingText.left(signaturePos);
                    }
                    
                    if (!msg.isRedacted) {
                        thinkingBlock["thinking"] = thinkingText;
                    }
                    thinkingBlock["signature"] = msg.signature;
                    content.append(thinkingBlock);
                    
                    messages.append(QJsonObject{{"role", "assistant"}, {"content", content}});
                } else if (msg.images && !msg.images->isEmpty()) {
                    QJsonArray content;
                    
                    if (!msg.content.isEmpty()) {
                        content.append(QJsonObject{{"type", "text"}, {"text", msg.content}});
                    }
                    
                    for (const auto &image : msg.images.value()) {
                        QJsonObject imageBlock;
                        imageBlock["type"] = "image";
                        
                        QJsonObject source;
                        if (image.isUrl) {
                            source["type"] = "url";
                            source["url"] = image.data;
                        } else {
                            source["type"] = "base64";
                            source["media_type"] = image.mediaType;
                            source["data"] = image.data;
                        }
                        imageBlock["source"] = source;
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
        return "Template for Anthropic's Claude models:\n\n"
               "{\n"
               "  \"system\": \"<system prompt>\",\n"
               "  \"messages\": [\n"
               "    {\"role\": \"user\", \"content\": \"<user message>\"},\n"
               "    {\"role\": \"assistant\", \"content\": \"<assistant response>\"}\n"
               "  ]\n"
               "}\n\n"
               "Formats content according to Claude API specifications.";
    }
    bool isSupportProvider(LLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::LLMCore::ProviderID::Claude:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
