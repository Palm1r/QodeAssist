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

#include "pluginllmcore/PromptTemplate.hpp"
#include <QJsonArray>

namespace QodeAssist::Templates {

class Alpaca : public PluginLLMCore::PromptTemplate
{
public:
    QString name() const override { return "Alpaca"; }
    PluginLLMCore::TemplateType type() const override { return PluginLLMCore::TemplateType::Chat; }
    QStringList stopWords() const override
    {
        return QStringList() << "### Instruction:" << "### Response:";
    }
    void prepareRequest(QJsonObject &request, const PluginLLMCore::ContextData &context) const override
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
    bool isSupportProvider(PluginLLMCore::ProviderID id) const override
    {
        switch (id) {
        case PluginLLMCore::ProviderID::Ollama:
        case PluginLLMCore::ProviderID::LMStudio:
        case PluginLLMCore::ProviderID::OpenRouter:
        case PluginLLMCore::ProviderID::OpenAICompatible:
        case PluginLLMCore::ProviderID::LlamaCpp:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
