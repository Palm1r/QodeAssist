// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QString>

#include "session/ContentBlock.hpp"

namespace QodeAssist::Session {

enum class MessageRole { User, Assistant, System };

struct Usage
{
    int promptTokens = 0;
    int completionTokens = 0;
    int cachedPromptTokens = 0;
    int reasoningTokens = 0;

    bool isEmpty() const
    {
        return promptTokens == 0 && completionTokens == 0 && cachedPromptTokens == 0
               && reasoningTokens == 0;
    }

    bool operator==(const Usage &other) const = default;

    friend QDebug operator<<(QDebug debug, const Usage &usage)
    {
        return debug.nospace() << "Usage(" << usage.promptTokens << ", " << usage.completionTokens
                               << ", " << usage.cachedPromptTokens << ", " << usage.reasoningTokens
                               << ")";
    }
};

struct Message
{
    MessageRole role = MessageRole::User;
    QString id;
    QList<ContentBlock> blocks;
    Usage usage;

    bool operator==(const Message &other) const = default;

    friend QDebug operator<<(QDebug debug, const Message &message)
    {
        return debug.nospace() << "Message(role=" << int(message.role) << ", id=" << message.id
                               << ", blocks=" << message.blocks << ", " << message.usage << ")";
    }
};

} // namespace QodeAssist::Session
