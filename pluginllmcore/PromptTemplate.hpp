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

    virtual QString endpoint() const { return {}; }

    virtual bool supportsToolHistory() const { return false; }
};
} // namespace QodeAssist::PluginLLMCore
