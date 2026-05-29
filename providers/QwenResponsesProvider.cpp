// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "QwenResponsesProvider.hpp"

#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

QwenResponsesProvider::QwenResponsesProvider(QObject *parent)
    : OpenAIResponsesProvider(parent)
{}

QString QwenResponsesProvider::name() const
{
    return "Qwen (OpenAI Response)";
}

QString QwenResponsesProvider::apiKey() const
{
    return Settings::providerSettings().qwenApiKey();
}

QString QwenResponsesProvider::url() const
{
    return "https://dashscope-intl.aliyuncs.com/compatible-mode/v1";
}

QFuture<QList<QString>> QwenResponsesProvider::getInstalledModels(const QString &)
{
    return QtFuture::makeReadyFuture(QList<QString>{});
}

PluginLLMCore::ProviderCapabilities QwenResponsesProvider::capabilities() const
{
    return PluginLLMCore::ProviderCapability::Tools | PluginLLMCore::ProviderCapability::Thinking
           | PluginLLMCore::ProviderCapability::Image;
}

} // namespace QodeAssist::Providers
