// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "session/ConversationHistory.hpp"

namespace QodeAssist::Chat {

class ChatModel;

class ChatHistoryBridge
{
public:
    static Session::ConversationHistory readHistory(const ChatModel *model);
    static void applyHistory(ChatModel *model, const Session::ConversationHistory &history);
};

} // namespace QodeAssist::Chat
