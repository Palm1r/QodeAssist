// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ConversationHistory.hpp"

namespace QodeAssist {

ConversationHistory::ConversationHistory(QObject *parent)
    : QObject(parent)
{}

ConversationHistory::~ConversationHistory() = default;

void ConversationHistory::append(Message message)
{
    m_messages.push_back(std::move(message));
    emit messageAdded(static_cast<int>(m_messages.size()) - 1);
}

void ConversationHistory::appendBlockToLast(std::unique_ptr<LLMQore::ContentBlock> block)
{
    if (m_messages.empty() || !block)
        return;

    m_messages.back().appendBlock(std::move(block));
    emit messageUpdated(static_cast<int>(m_messages.size()) - 1);
}

void ConversationHistory::appendTextDeltaToLast(const QString &delta)
{
    if (m_messages.empty() || delta.isEmpty())
        return;

    auto &last = m_messages.back();
    if (auto *text = last.lastBlockOfType<LLMQore::TextContent>()) {
        text->appendText(delta);
    } else {
        last.appendBlock(std::make_unique<LLMQore::TextContent>(delta));
    }
    emit messageUpdated(static_cast<int>(m_messages.size()) - 1);
}

void ConversationHistory::appendThinkingDeltaToLast(const QString &delta, const QString &signature)
{
    if (m_messages.empty() || (delta.isEmpty() && signature.isEmpty()))
        return;

    auto &last = m_messages.back();
    auto *thinking = last.lastBlockOfType<LLMQore::ThinkingContent>();
    if (!thinking) {
        auto fresh = std::make_unique<LLMQore::ThinkingContent>(delta, signature);
        last.appendBlock(std::move(fresh));
    } else {
        if (!delta.isEmpty())
            thinking->appendThinking(delta);
        if (!signature.isEmpty())
            thinking->setSignature(signature);
    }
    emit messageUpdated(static_cast<int>(m_messages.size()) - 1);
}

void ConversationHistory::clear()
{
    if (m_messages.empty())
        return;

    m_messages.clear();
    emit cleared();
}

void ConversationHistory::resetTo(int index)
{
    if (index < 0)
        index = 0;
    if (static_cast<size_t>(index) >= m_messages.size())
        return;

    m_messages.resize(static_cast<size_t>(index));
    emit reset();
}

} // namespace QodeAssist
