/* 
 * Copyright (C) 2024 Petr Mironychev
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

class Llama2 : public LLMCore::PromptTemplate
{
public:
    QString name() const override { return "Llama 2"; }
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Chat; }
    QStringList stopWords() const override { return QStringList() << "[INST]"; }
    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray messages;

        QString fullContent;

        if (context.systemPrompt) {
            fullContent
                += QString("[INST]<<SYS>>\n%1\n<</SYS>>[/INST]\n").arg(context.systemPrompt.value());
        }

        if (context.history) {
            for (const auto &msg : context.history.value()) {
                if (msg.role == "user") {
                    fullContent += QString("[INST]%1[/INST]\n").arg(msg.content);
                } else if (msg.role == "assistant") {
                    fullContent += msg.content + "\n";
                }
            }
        }

        messages.append(QJsonObject{{"role", "user"}, {"content", fullContent}});

        request["messages"] = messages;
    }
    QString description() const override
    {
        return "The message will contain the following tokens: [INST]%1[/INST]\n";
    }
    bool isSupportProvider(LLMCore::ProviderID id) const override
    {
        switch (id) {
        case QodeAssist::LLMCore::ProviderID::Ollama:
        case QodeAssist::LLMCore::ProviderID::LMStudio:
        case QodeAssist::LLMCore::ProviderID::OpenRouter:
        case QodeAssist::LLMCore::ProviderID::OpenAICompatible:
            return true;
        default:
            return false;
        }
    }
};

} // namespace QodeAssist::Templates
