// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "llmcore/PromptTemplate.hpp"

namespace QodeAssist::Templates {

class DeepSeekCoderFim : public LLMQore::PromptTemplate
{
public:
    LLMQore::TemplateType type() const override { return LLMQore::TemplateType::Fim; }
    QString name() const override { return "DeepSeekCoder FIM"; }
    QString promptTemplate() const override
    {
        return "<｜fim▁begin｜>%1<｜fim▁hole｜>%2<｜fim▁end｜>";
    }
    QStringList stopWords() const override { return QStringList(); }
    void prepareRequest(QJsonObject &request, const LLMQore::ContextData &context) const override
    {
        QString formattedPrompt = promptTemplate().arg(context.prefix, context.suffix);
        request["prompt"] = formattedPrompt;
    }
    QString description() const override
    {
        return "The message will contain the following tokens: "
               "<｜fim▁begin｜>%1<｜fim▁hole｜>%2<｜fim▁end｜>";
    }
};

} // namespace QodeAssist::Templates
