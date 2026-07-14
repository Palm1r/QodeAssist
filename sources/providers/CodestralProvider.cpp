// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "CodestralProvider.hpp"

#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

CodestralProvider::CodestralProvider(QObject *parent)
    : MistralAIProvider(parent)
{}

QString CodestralProvider::name() const
{
    return "Codestral";
}

QString CodestralProvider::apiKey() const
{
    return Settings::providerSettings().codestralApiKey();
}

QString CodestralProvider::url() const
{
    return "https://codestral.mistral.ai";
}

Providers::ProviderCapabilities CodestralProvider::capabilities() const
{
    return Providers::ProviderCapability::Tools | Providers::ProviderCapability::Image;
}

} // namespace QodeAssist::Providers
