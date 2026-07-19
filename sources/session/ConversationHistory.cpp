// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/ConversationHistory.hpp"

namespace QodeAssist::Session {

void ConversationHistory::append(const Message &message)
{
    m_messages.append(message);
}

qsizetype ConversationHistory::size() const
{
    return m_messages.size();
}

const QList<Message> &ConversationHistory::messages() const
{
    return m_messages;
}

const Message &ConversationHistory::at(qsizetype index) const
{
    return m_messages.at(index);
}

const Message &ConversationHistory::last() const
{
    return m_messages.last();
}

Message *ConversationHistory::lastMessage()
{
    return m_messages.isEmpty() ? nullptr : &m_messages.last();
}

void ConversationHistory::visitBlocks(const std::function<void(ContentBlock &)> &visit)
{
    for (Message &message : m_messages) {
        for (ContentBlock &block : message.blocks)
            visit(block);
    }
}

} // namespace QodeAssist::Session
