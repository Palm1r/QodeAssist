// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

namespace QodeAssist::PluginLLMCore {

enum class ProviderID {
    Any,
    Ollama,
    LMStudio,
    Claude,
    OpenAI,
    OpenAICompatible,
    OpenAIResponses,
    MistralAI,
    OpenRouter,
    GoogleAI,
    LlamaCpp
};
}
