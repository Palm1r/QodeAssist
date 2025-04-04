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

#include "llmcore/PromptTemplateManager.hpp"
#include "templates/Alpaca.hpp"
#include "templates/ChatML.hpp"
#include "templates/Claude.hpp"
#include "templates/CodeLlamaFim.hpp"
#include "templates/CodeLlamaQMLFim.hpp"
#include "templates/MistralAI.hpp"
#include "templates/Ollama.hpp"
#include "templates/OpenAI.hpp"
#include "templates/OpenAICompatible.hpp"
// #include "templates/CustomFimTemplate.hpp"
// #include "templates/DeepSeekCoderFim.hpp"
#include "templates/GoogleAI.hpp"
#include "templates/Llama2.hpp"
#include "templates/Llama3.hpp"
#include "templates/LlamaCppFim.hpp"
#include "templates/Qwen.hpp"
#include "templates/StarCoder2Fim.hpp"

namespace QodeAssist::Templates {

inline void registerTemplates()
{
    auto &templateManager = LLMCore::PromptTemplateManager::instance();
    templateManager.registerTemplate<OllamaChat>();
    templateManager.registerTemplate<OllamaFim>();
    templateManager.registerTemplate<CodeLlamaFim>();
    templateManager.registerTemplate<Claude>();
    templateManager.registerTemplate<OpenAI>();
    templateManager.registerTemplate<MistralAIFim>();
    templateManager.registerTemplate<MistralAIChat>();
    templateManager.registerTemplate<CodeLlamaQMLFim>();
    templateManager.registerTemplate<ChatML>();
    templateManager.registerTemplate<Llama2>();
    templateManager.registerTemplate<Llama3>();
    templateManager.registerTemplate<StarCoder2Fim>();
    // templateManager.registerTemplate<DeepSeekCoderFim>();
    // templateManager.registerTemplate<CustomTemplate>();
    templateManager.registerTemplate<QwenFim>();
    templateManager.registerTemplate<OpenAICompatible>();
    templateManager.registerTemplate<Alpaca>();
    templateManager.registerTemplate<GoogleAI>();
    templateManager.registerTemplate<LlamaCppFim>();
}

} // namespace QodeAssist::Templates
