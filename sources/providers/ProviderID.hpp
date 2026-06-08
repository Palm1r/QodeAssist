// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

namespace QodeAssist::Providers {

enum class ProviderID : int {
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
};

} // namespace QodeAssist::Providers
