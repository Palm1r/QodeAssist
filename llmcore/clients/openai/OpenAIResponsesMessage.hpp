/*
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <llmcore/core/ContentBlocks.hpp>

namespace QodeAssist::LLMCore {

class OpenAIResponsesMessage : public QObject
{
    Q_OBJECT
public:
    explicit OpenAIResponsesMessage(QObject *parent = nullptr);

    void handleContentDelta(const QString &text);
    void handleToolCallStart(const QString &callId, const QString &name);
    void handleToolCallDelta(const QString &callId, const QString &argumentsDelta);
    void handleToolCallComplete(const QString &callId);
    void handleReasoningStart(const QString &itemId);
    void handleReasoningDelta(const QString &itemId, const QString &text);
    void handleReasoningComplete(const QString &itemId);
    void handleStatus(const QString &status);

    QList<QJsonObject> toItemsFormat() const;
    QJsonArray createToolResultItems(const QHash<QString, QString> &toolResults) const;

    MessageState state() const noexcept { return m_state; }
    QString accumulatedText() const;
    QList<ToolUseContent *> getCurrentToolUseContent() const;
    QList<ThinkingContent *> getCurrentThinkingContent() const;

    bool hasToolCalls() const noexcept { return !m_toolCalls.isEmpty(); }
    bool hasThinkingContent() const noexcept { return !m_thinkingBlocks.isEmpty(); }

    void startNewContinuation();

private:
    QString m_status;
    MessageState m_state = MessageState::Building;
    QList<ContentBlock *> m_items;
    QHash<QString, QString> m_pendingToolArguments;
    QHash<QString, ToolUseContent *> m_toolCalls;
    QHash<QString, ThinkingContent *> m_thinkingBlocks;

    void updateStateFromStatus();
    TextContent *getOrCreateTextItem();
};

} // namespace QodeAssist::LLMCore
