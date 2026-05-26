// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Message.hpp"

namespace QodeAssist {

QString Message::text() const
{
    QString out;
    for (const auto &block : m_blocks) {
        if (auto *t = dynamic_cast<LLMQore::TextContent *>(block.get())) {
            if (!out.isEmpty())
                out += QStringLiteral("\n\n");
            out += t->text();
        }
    }
    return out;
}

bool Message::hasToolUse() const
{
    for (const auto &block : m_blocks) {
        if (dynamic_cast<LLMQore::ToolUseContent *>(block.get()))
            return true;
    }
    return false;
}

} // namespace QodeAssist
