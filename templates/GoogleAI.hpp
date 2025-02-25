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

#include <QJsonArray>
#include <QJsonObject>

#include "llmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class GoogleAI : public LLMCore::PromptTemplate
{
public:
    LLMCore::TemplateType type() const override { return LLMCore::TemplateType::Chat; }
    QString name() const override { return "Google AI"; }
    QStringList stopWords() const override { return QStringList(); }

    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray contents;

        if (context.systemPrompt && !context.systemPrompt->isEmpty()) {
            request["system_instruction"] = QJsonObject{
                {"parts", QJsonObject{{"text", context.systemPrompt.value()}}}};
        }

        for (const auto &msg : context.history.value()) {
            QJsonObject content;
            QJsonArray parts;

            parts.append(QJsonObject{{"text", msg.content}});

            QString role = msg.role;
            if (role == "assistant") {
                role = "model";
            }

            content["role"] = role;
            content["parts"] = parts;
            contents.append(content);
        }

        request["contents"] = contents;
    }

    QString description() const override
    {
        return "Template for Google AI models (Gemini):\n\n"
               "{\n"
               "  \"system_instruction\": {\"parts\": {\"text\": \"<system prompt>\"}},\n"
               "  \"contents\": [\n"
               "    {\n"
               "      \"role\": \"user\",\n"
               "      \"parts\": [{\"text\": \"<user message>\"}]\n"
               "    },\n"
               "    {\n"
               "      \"role\": \"model\",\n"
               "      \"parts\": [{\"text\": \"<assistant response>\"}]\n"
               "    }\n"
               "  ]\n"
               "}\n\n"
               "Supports proper role mapping, including model/user roles.";
    }

    bool isSupportProvider(LLMCore::ProviderID id) const override
    {
        return id == QodeAssist::LLMCore::ProviderID::GoogleAI;
    }
};

} // namespace QodeAssist::Templates
