// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pluginllmcore/ProvidersManager.hpp"
#include "providers/ClaudeProvider.hpp"
#include "providers/DeepSeekProvider.hpp"
#include "providers/CodestralProvider.hpp"
#include "providers/GoogleAIProvider.hpp"
#include "providers/LMStudioProvider.hpp"
#include "providers/LMStudioResponsesProvider.hpp"
#include "providers/LlamaCppProvider.hpp"
#include "providers/MistralAIProvider.hpp"
#include "providers/OllamaCompatProvider.hpp"
#include "providers/OllamaProvider.hpp"
#include "providers/OpenAICompatProvider.hpp"
#include "providers/OpenAIProvider.hpp"
#include "providers/OpenAIResponsesProvider.hpp"
#include "providers/OpenRouterAIProvider.hpp"
#include "providers/QwenProvider.hpp"
#include "providers/QwenResponsesProvider.hpp"

namespace QodeAssist::Providers {

inline void registerProviders()
{
    auto &providerManager = PluginLLMCore::ProvidersManager::instance();
    providerManager.registerProvider<OllamaProvider>();
    providerManager.registerProvider<OllamaCompatProvider>();
    providerManager.registerProvider<ClaudeProvider>();
    providerManager.registerProvider<OpenAIProvider>();
    providerManager.registerProvider<OpenAIResponsesProvider>();
    providerManager.registerProvider<OpenAICompatProvider>();
    providerManager.registerProvider<LMStudioProvider>();
    providerManager.registerProvider<LMStudioResponsesProvider>();
    providerManager.registerProvider<OpenRouterProvider>();
    providerManager.registerProvider<MistralAIProvider>();
    providerManager.registerProvider<GoogleAIProvider>();
    providerManager.registerProvider<LlamaCppProvider>();
    providerManager.registerProvider<CodestralProvider>();
    providerManager.registerProvider<QwenProvider>();
    providerManager.registerProvider<QwenResponsesProvider>();
    providerManager.registerProvider<DeepSeekProvider>();
}

} // namespace QodeAssist::Providers
