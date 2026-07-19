// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>

#include "session/Message.hpp"

namespace QodeAssist::Session {

class ConversationHistory
{
public:
    void append(const Message &message);

    qsizetype size() const;
    const QList<Message> &messages() const;
    const Message &at(qsizetype index) const;

    bool operator==(const ConversationHistory &other) const = default;

    friend QDebug operator<<(QDebug debug, const ConversationHistory &history)
    {
        return debug.nospace() << "History(" << history.m_messages << ")";
    }

private:
    QList<Message> m_messages;
};

} // namespace QodeAssist::Session
