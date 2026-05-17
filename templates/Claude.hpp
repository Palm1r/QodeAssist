// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>

#include "pluginllmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class Claude : public PluginLLMCore::PromptTemplate
{
public:
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::Chat; }
    QString name() const override { return "Claude"; }
    QStringList stopWords() const override { return QStringList(); }
    bool supportsToolHistory() const override { return true; }
    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        if (context.systemPrompt) {
            request["system"] = context.systemPrompt.value();
        }

        if (context.history) {
            int toolResultUserIdx = -1;
            for (const auto &msg : context.history.value()) {
                if (msg.role == "system") continue;

                if (!msg.toolCalls.isEmpty()) {
                    toolResultUserIdx = -1;
                    QJsonArray content;
                    if (!msg.content.isEmpty()) {
                        content.append(QJsonObject{{"type", "text"}, {"text", msg.content}});
                    }
                    for (const auto &call : msg.toolCalls) {
                        content.append(QJsonObject{
                            {"type", "tool_use"},
                            {"id", call.id},
                            {"name", call.name},
                            {"input", call.arguments}});
                    }
                    messages.append(QJsonObject{{"role", "assistant"}, {"content", content}});
                    continue;
                }

                if (msg.role == "tool") {
                    QJsonObject resultBlock{
                        {"type", "tool_result"},
                        {"tool_use_id", msg.toolCallId},
                        {"content", msg.content}};
                    if (toolResultUserIdx >= 0) {
                        QJsonObject userMsg = messages[toolResultUserIdx].toObject();
                        QJsonArray content = userMsg["content"].toArray();
                        content.append(resultBlock);
                        userMsg["content"] = content;
                        messages[toolResultUserIdx] = userMsg;
                    } else {
                        messages.append(QJsonObject{
                            {"role", "user"}, {"content", QJsonArray{resultBlock}}});
                        toolResultUserIdx = messages.size() - 1;
                    }
                    continue;
                }

                toolResultUserIdx = -1;

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
    bool isSupportProvider(PluginLLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::PluginLLMCore::ProviderID::Claude:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
