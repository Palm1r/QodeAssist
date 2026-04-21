// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "MistralAIProvider.hpp"

namespace QodeAssist::Providers {

class CodestralProvider : public MistralAIProvider
{
public:
    explicit CodestralProvider(QObject *parent = nullptr);

    QString name() const override;
    QString url() const override;
    QString apiKey() const override;
    PluginLLMCore::ProviderCapabilities capabilities() const override;
};

} // namespace QodeAssist::Providers
