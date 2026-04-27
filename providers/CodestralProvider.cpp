// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

PluginLLMCore::ProviderCapabilities CodestralProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Image;
}

} // namespace QodeAssist::Providers
