// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>

#include <optional>

#include "session/ContentBlock.hpp"
#include "session/ConversationHistory.hpp"
#include "session/TurnContext.hpp"

namespace QodeAssist::Session {

struct TurnRequest
{
    QList<ContentBlock> userBlocks;
    const ConversationHistory *history = nullptr;
    std::optional<TurnContext> context;
};

} // namespace QodeAssist::Session
