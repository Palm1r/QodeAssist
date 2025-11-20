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

class OllamaFim : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::FIM; }
    QString name() const override { return "Ollama FIM"; }
    QStringList stopWords() const override { return QStringList() << "<EOT>"; }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
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
    bool isSupportProvider(LLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::LLMCore::ProviderID::Ollama:
            return true;
        default:
            return false;
        }
    }
};

class OllamaChat : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Chat; }
    QString name() const override { return "Ollama Chat"; }
    QStringList stopWords() const override { return QStringList(); }

    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
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
                messageObj["content"] = msg.content;
                
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
    bool isSupportProvider(LLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::LLMCore::ProviderID::Ollama:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
