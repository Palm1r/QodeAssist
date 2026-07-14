// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include "llmcore/ContextData.hpp"
#include "providers/ProviderID.hpp"

namespace QodeAssist::Templates {

enum class TemplateType { Chat, FIM, FIMOnChat };

class PromptTemplate
{
public:
    virtual ~PromptTemplate() = default;
    virtual TemplateType type() const = 0;
    virtual QString name() const = 0;
    virtual QStringList stopWords() const = 0;
    virtual void prepareRequest(QJsonObject &request, const LLMCore::ContextData &context) const = 0;
    virtual QString description() const = 0;
    virtual bool isSupportProvider(Providers::ProviderID id) const = 0;

    virtual QString endpoint() const { return {}; }

    virtual bool supportsToolHistory() const { return false; }
};
} // namespace QodeAssist::Templates
