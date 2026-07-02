// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <LLMQore/ContentBlocks.hpp>

#include <QObject>

#include <memory>
#include <vector>

#include "Message.hpp"

namespace QodeAssist {

class ConversationHistory : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConversationHistory)
public:
    explicit ConversationHistory(QObject *parent = nullptr);
    ~ConversationHistory() override;

    const std::vector<Message> &messages() const noexcept { return m_messages; }
    int size() const noexcept { return static_cast<int>(m_messages.size()); }
    bool isEmpty() const noexcept { return m_messages.empty(); }

    void append(Message message);

    void appendBlockToLast(std::unique_ptr<LLMQore::ContentBlock> block);

    void appendTextDeltaToLast(const QString &delta);
    void appendThinkingBlockToLast(const QString &thinking, const QString &signature = QString());

    void clear();
    void resetTo(int index);

signals:
    void messageAdded(int index);
    void messageUpdated(int index);
    void cleared();
    void reset();

private:
    std::vector<Message> m_messages;
};

} // namespace QodeAssist
