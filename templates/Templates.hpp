// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pluginllmcore/PromptTemplateManager.hpp"
#include "templates/Alpaca.hpp"
#include "templates/ChatML.hpp"
#include "templates/Claude.hpp"
#include "templates/CodeLlamaFim.hpp"
#include "templates/CodeLlamaQMLFim.hpp"
#include "templates/MistralAI.hpp"
#include "templates/Ollama.hpp"
#include "templates/OpenAI.hpp"
#include "templates/OpenAICompatible.hpp"
#include "templates/OpenAIResponses.hpp"
#include "templates/GoogleAI.hpp"
#include "templates/Llama2.hpp"
#include "templates/Llama3.hpp"
#include "templates/LlamaCppFim.hpp"
#include "templates/Qwen25CoderFIM.hpp"
#include "templates/Qwen3CoderFIM.hpp"
#include "templates/StarCoder2Fim.hpp"

namespace QodeAssist::Templates {

inline void registerTemplates()
{
    auto &templateManager = PluginLLMCore::PromptTemplateManager::instance();
    templateManager.registerTemplate<OllamaChat>();
    templateManager.registerTemplate<OllamaFim>();
    templateManager.registerTemplate<CodeLlamaFim>();
    templateManager.registerTemplate<Claude>();
    templateManager.registerTemplate<OpenAI>();
    templateManager.registerTemplate<OpenAIResponses>();
    templateManager.registerTemplate<MistralAIFim>();
    templateManager.registerTemplate<MistralAIChat>();
    templateManager.registerTemplate<CodeLlamaQMLFim>();
    templateManager.registerTemplate<ChatML>();
    templateManager.registerTemplate<Llama2>();
    templateManager.registerTemplate<Llama3>();
    templateManager.registerTemplate<StarCoder2Fim>();
    templateManager.registerTemplate<Qwen25CoderFIM>();
    templateManager.registerTemplate<Qwen3CoderFIM>();
    templateManager.registerTemplate<OpenAICompatible>();
    templateManager.registerTemplate<Alpaca>();
    templateManager.registerTemplate<GoogleAI>();
    templateManager.registerTemplate<LlamaCppFim>();
}

} // namespace QodeAssist::Templates
