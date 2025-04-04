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
#include <QJsonArray>

namespace QodeAssist::Templates {

class Alpaca : public LLMCore::PromptTemplate
{
public:
    QString name() const override { return "Alpaca"; }
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Chat; }
    QStringList stopWords() const override
    {
        return QStringList() << "### Instruction:" << "### Response:";
    }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        QString fullContent;

        if (context.systemPrompt) {
            fullContent += context.systemPrompt.value() + "\n\n";
        }

        if (context.history) {
            for (const auto &msg : context.history.value()) {
                if (msg.role == "user") {
                    fullContent += QString("### Instruction:\n%1\n\n").arg(msg.content);
                } else if (msg.role == "assistant") {
                    fullContent += QString("### Response:\n%1\n\n").arg(msg.content);
                }
            }
        }

        messages.append(QJsonObject{{"role", "user"}, {"content", fullContent}});

        request["messages"] = messages;
    }
    QString description() const override
    {
        return "Template for models using Alpaca instruction format:\n\n"
               "{\n"
               "  \"messages\": [\n"
               "    {\n"
               "      \"role\": \"user\",\n"
               "      \"content\": \"<system prompt>\\n\\n"
               "### Instruction:\\n<user message>\\n\\n"
               "### Response:\\n<assistant response>\\n\\n\"\n"
               "    }\n"
               "  ]\n"
               "}\n\n"
               "Combines all messages into a single formatted prompt.";
    }
    bool isSupportProvider(LLMCore::ProviderID id) const override
    {
        switch (id) {
        case LLMCore::ProviderID::Ollama:
        case LLMCore::ProviderID::LMStudio:
        case LLMCore::ProviderID::OpenRouter:
        case LLMCore::ProviderID::OpenAICompatible:
        case LLMCore::ProviderID::LlamaCpp:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
