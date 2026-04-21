// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "PromptTemplate.hpp"
#include <QString>

namespace QodeAssist::PluginLLMCore {

class IPromptProvider
{
public:
    virtual ~IPromptProvider() = default;

    virtual PromptTemplate *getTemplateByName(const QString &templateName) const = 0;

    virtual QStringList templatesNames() const = 0;

    virtual QStringList getTemplatesForProvider(ProviderID id) const = 0;
};

} // namespace QodeAssist::PluginLLMCore
