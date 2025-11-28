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

            if (msg.isThinking) {
                if (!msg.content.isEmpty()) {
                    QJsonObject thinkingPart;
                    thinkingPart["text"] = msg.content;
                    thinkingPart["thought"] = true;
                    parts.append(thinkingPart);
                }
                
                if (!msg.signature.isEmpty()) {
                    QJsonObject signaturePart;
                    signaturePart["thoughtSignature"] = msg.signature;
                    parts.append(signaturePart);
                }
                
                if (parts.isEmpty()) {
                    continue;
                }
                
                content["role"] = "model";
            } else {
                if (!msg.content.isEmpty()) {
                    parts.append(QJsonObject{{"text", msg.content}});
                }

                if (msg.images && !msg.images->isEmpty()) {
                    for (const auto &image : msg.images.value()) {
                        QJsonObject imagePart;
                        
                        if (image.isUrl) {
                            QJsonObject fileData;
                            fileData["mime_type"] = image.mediaType;
                            fileData["file_uri"] = image.data;
                            imagePart["file_data"] = fileData;
                        } else {
                            QJsonObject inlineData;
                            inlineData["mime_type"] = image.mediaType;
                            inlineData["data"] = image.data;
                            imagePart["inline_data"] = inlineData;
                        }
                        
                        parts.append(imagePart);
                    }
                }

                QString role = msg.role;
                if (role == "assistant") {
                    role = "model";
                }
                
                content["role"] = role;
            }

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
               "      \"parts\": [\n"
               "        {\"text\": \"<thinking>\", \"thought\": true},\n"
               "        {\"thoughtSignature\": \"<signature>\"},\n"
               "        {\"text\": \"<assistant response>\"}\n"
               "      ]\n"
               "    }\n"
               "  ]\n"
               "}\n\n"
               "Supports proper role mapping (model/user roles), images, and thinking blocks.";
    }

    bool isSupportProvider(LLMCore::ProviderID id) const override
    {
        return id == QodeAssist::LLMCore::ProviderID::GoogleAI;
    }
};

} // namespace QodeAssist::Templates
