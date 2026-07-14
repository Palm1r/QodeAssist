// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "templates/PromptTemplate.hpp"
#include <QString>

namespace QodeAssist::Templates {

class IPromptProvider
{
public:
    virtual ~IPromptProvider() = default;

    virtual PromptTemplate *getTemplateByName(const QString &templateName) const = 0;

    virtual QStringList templatesNames() const = 0;

    virtual QStringList getTemplatesForProvider(Providers::ProviderID id) const = 0;
};

} // namespace QodeAssist::Templates
