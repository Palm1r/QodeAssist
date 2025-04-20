/* 
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "llmcore/ProvidersManager.hpp"
#include "providers/ClaudeProvider.hpp"
#include "providers/CodestralProvider.hpp"
#include "providers/GoogleAIProvider.hpp"
#include "providers/LMStudioProvider.hpp"
#include "providers/LlamaCppProvider.hpp"
#include "providers/MistralAIProvider.hpp"
#include "providers/OllamaProvider.hpp"
#include "providers/OpenAICompatProvider.hpp"
#include "providers/OpenAIProvider.hpp"
#include "providers/OpenRouterAIProvider.hpp"

namespace QodeAssist::Providers {

inline void registerProviders()
{
    auto &providerManager = LLMCore::ProvidersManager::instance();
    providerManager.registerProvider<OllamaProvider>();
    providerManager.registerProvider<ClaudeProvider>();
    providerManager.registerProvider<OpenAIProvider>();
    providerManager.registerProvider<OpenAICompatProvider>();
    providerManager.registerProvider<LMStudioProvider>();
    providerManager.registerProvider<OpenRouterProvider>();
    providerManager.registerProvider<MistralAIProvider>();
    providerManager.registerProvider<GoogleAIProvider>();
    providerManager.registerProvider<LlamaCppProvider>();
    providerManager.registerProvider<CodestralProvider>();
}

} // namespace QodeAssist::Providers
