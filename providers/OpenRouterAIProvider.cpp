// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "OpenRouterAIProvider.hpp"

#include "settings/ProviderSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace QodeAssist::Providers {

OpenRouterProvider::OpenRouterProvider(QObject *parent)
    : OpenAICompatProvider(parent)
{}

QString OpenRouterProvider::name() const
{
    return "OpenRouter";
}

QString OpenRouterProvider::apiKey() const
{
    return Settings::providerSettings().openRouterApiKey();
}

QString OpenRouterProvider::url() const
{
    return "https://openrouter.ai/api";
}

PluginLLMCore::ProviderID OpenRouterProvider::providerID() const
{
    return PluginLLMCore::ProviderID::OpenRouter;
}

} // namespace QodeAssist::Providers
