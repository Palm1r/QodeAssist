// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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
    LlamaCpp,
    Qwen,
    DeepSeek
};
}
