// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonArray>
#include <QJsonObject>

#include "templates/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class GoogleAI : public Templates::PromptTemplate
{
public:
    Templates::TemplateType type() const override { return Templates::TemplateType::Chat; }
    QString name() const override { return "Google AI"; }
    QStringList stopWords() const override { return QStringList(); }
    bool supportsToolHistory() const override { return true; }

    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        QJsonArray contents;

        if (context.systemPrompt && !context.systemPrompt->isEmpty()) {
            request["system_instruction"] = QJsonObject{
                {"parts", QJsonObject{{"text", context.systemPrompt.value()}}}};
        }

        int toolResultIdx = -1;
        for (const auto &msg : context.history.value()) {
            if (!msg.toolCalls.isEmpty()) {
                toolResultIdx = -1;
                QJsonArray callParts;
                if (!msg.content.isEmpty()) {
                    callParts.append(QJsonObject{{"text", msg.content}});
                }
                for (const auto &call : msg.toolCalls) {
                    callParts.append(QJsonObject{
                        {"functionCall",
                         QJsonObject{{"name", call.name}, {"args", call.arguments}}}});
                }
                contents.append(QJsonObject{{"role", "model"}, {"parts", callParts}});
                continue;
            }

            if (msg.role == "tool") {
                QJsonObject responsePart{
                    {"functionResponse",
                     QJsonObject{
                         {"name", msg.toolName},
                         {"response", QJsonObject{{"result", msg.content}}}}}};
                if (toolResultIdx >= 0) {
                    QJsonObject fnMsg = contents[toolResultIdx].toObject();
                    QJsonArray fnParts = fnMsg["parts"].toArray();
                    fnParts.append(responsePart);
                    fnMsg["parts"] = fnParts;
                    contents[toolResultIdx] = fnMsg;
                } else {
                    contents.append(
                        QJsonObject{{"role", "function"}, {"parts", QJsonArray{responsePart}}});
                    toolResultIdx = contents.size() - 1;
                }
                continue;
            }

            toolResultIdx = -1;

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

    bool isSupportProvider(Providers::ProviderID id) const override
    {
        return id == QodeAssist::Providers::ProviderID::GoogleAI;
    }
};

} // namespace QodeAssist::Templates
