// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonArray>

#include "templates/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class LlamaCppFim : public Templates::PromptTemplate
{
public:
    Templates::TemplateType type() const override { return Templates::TemplateType::FIM; }
    QString name() const override { return "llama.cpp FIM"; }
    QString endpoint() const override { return QStringLiteral("/infill"); }
    QStringList stopWords() const override { return {}; }

    void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const override
    {
        request["input_prefix"] = context.prefix.value_or("");
        request["input_suffix"] = context.suffix.value_or("");

        if (context.filesMetadata && !context.filesMetadata->isEmpty()) {
            QJsonArray filesArray;
            for (const auto &file : *context.filesMetadata) {
                QJsonObject fileObj;
                fileObj["filename"] = file.filePath;
                fileObj["text"] = file.content;
                filesArray.append(fileObj);
            }
            request["input_extra"] = filesArray;
        }
    }

    QString description() const override
    {
        return "Default llama.cpp FIM (Fill-in-Middle) /infill template with native format:\n\n"
               "{\n"
               "  \"input_prefix\": \"<code prefix>\",\n"
               "  \"input_suffix\": \"<code suffix>\",\n"
               "  \"input_extra\": \"<system prompt>\"\n"
               "}\n\n"
               "Recommended for models with FIM capability.";
    }

    bool isSupportProvider(Providers::ProviderID id) const override
    {
        return id == QodeAssist::Providers::ProviderID::LlamaCpp;
    }
};

} // namespace QodeAssist::Templates
