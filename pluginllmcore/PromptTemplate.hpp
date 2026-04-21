// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include "ContextData.hpp"
#include "ProviderID.hpp"

namespace QodeAssist::PluginLLMCore {

enum class TemplateType { Chat, FIM, FIMOnChat };

class PromptTemplate
{
public:
    virtual ~PromptTemplate() = default;
    virtual TemplateType type() const = 0;
    virtual QString name() const = 0;
    virtual QStringList stopWords() const = 0;
    virtual void prepareRequest(QJsonObject &request, const ContextData &context) const = 0;
    virtual QString description() const = 0;
    virtual bool isSupportProvider(ProviderID id) const = 0;

    // Endpoint path this template expects to be sent to. Empty string
    // (default) means "let the provider's client use its standard chat
    // path" (/chat/completions, /api/chat, /v1/messages, ...). Templates
    // producing non-chat payload shapes (e.g. {prompt, suffix} for
    // Mistral FIM, {input_prefix, input_suffix} for llama.cpp infill)
    // must override this to the path their payload is valid for.
    virtual QString endpoint() const { return {}; }
};
} // namespace QodeAssist::PluginLLMCore
