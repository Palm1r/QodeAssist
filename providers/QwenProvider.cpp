// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "QwenProvider.hpp"

#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

QwenProvider::QwenProvider(QObject *parent)
    : OpenAICompatProvider(parent)
{}

QString QwenProvider::name() const
{
    return "Qwen (OpenAI)";
}

QString QwenProvider::apiKey() const
{
    return Settings::providerSettings().qwenApiKey();
}

QString QwenProvider::url() const
{
    return "https://dashscope-intl.aliyuncs.com/compatible-mode/v1";
}

PluginLLMCore::ProviderID QwenProvider::providerID() const
{
    return PluginLLMCore::ProviderID::Qwen;
}

} // namespace QodeAssist::Providers
