// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include "ContextData.hpp"
#include "ProviderID.hpp"

namespace QodeAssist::Templates {

using Providers::ProviderID;

enum class PromptShape {
    Chat,
    Fim,
};

class PromptTemplate
{
public:
    PromptTemplate() = default;
    virtual ~PromptTemplate() = default;

    PromptTemplate(const PromptTemplate &) = delete;
    PromptTemplate &operator=(const PromptTemplate &) = delete;
    PromptTemplate(PromptTemplate &&) = delete;
    PromptTemplate &operator=(PromptTemplate &&) = delete;

    virtual QString name() const = 0;
    virtual void prepareRequest(QJsonObject &request, const ContextData &context) const = 0;
    virtual QString description() const = 0;
    virtual bool isSupportProvider(ProviderID id) const = 0;
    virtual PromptShape shape() const { return PromptShape::Chat; }

    virtual bool isSupportModel(const QString & /*modelName*/) const { return true; }

    [[nodiscard]] virtual bool buildFullRequest(
        QJsonObject &request, const ContextData &context) const
    {
        prepareRequest(request, context);
        return true;
    }
};
} // namespace QodeAssist::Templates
