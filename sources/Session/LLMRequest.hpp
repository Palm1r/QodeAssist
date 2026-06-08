// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

#include <optional>
#include <vector>

#include "Message.hpp"

namespace QodeAssist {

struct LLMRequest
{
    QString systemPrompt;
    std::vector<Message> history;
    bool toolsEnabled = false;
    bool thinkingEnabled = false;

    std::optional<QString> fimPrefix;
    std::optional<QString> fimSuffix;

    LLMRequest() = default;
    LLMRequest(const LLMRequest &) = delete;
    LLMRequest &operator=(const LLMRequest &) = delete;
    LLMRequest(LLMRequest &&) noexcept = default;
    LLMRequest &operator=(LLMRequest &&) noexcept = default;
};

} // namespace QodeAssist
