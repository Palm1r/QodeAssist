// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "providers/OpenAICompatProvider.hpp"

namespace QodeAssist::Providers {

class OpenRouterProvider : public OpenAICompatProvider
{
public:
    explicit OpenRouterProvider(QObject *parent = nullptr);

    QString name() const override;
    QString url() const override;
    QString apiKey() const override;
    PluginLLMCore::ProviderID providerID() const override;
};

} // namespace QodeAssist::Providers
