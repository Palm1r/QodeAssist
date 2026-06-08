// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "Provider.hpp"

namespace QodeAssist::PluginLLMCore {

class IProviderRegistry
{
public:
    virtual ~IProviderRegistry() = default;

    virtual Provider *getProviderByName(const QString &providerName) = 0;

    virtual QStringList providersNames() const = 0;
};

} // namespace QodeAssist::PluginLLMCore
