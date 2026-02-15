/*
 * Copyright (C) 2025 Petr Mironychev
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

#include <llmcore/ContentBlocks.hpp>

namespace QodeAssist::LLMCore {

struct StreamEvent {
    enum class Type {
        None,
        MessageStart,
        TextDelta,
        ThinkingBlockComplete,
        RedactedThinkingBlockComplete,
        MessageComplete
    };

    Type type = Type::None;
    QString text;
    QString thinking;
    QString signature;
    int blockIndex = -1;
};

class ClaudeResponse : public QObject
{
    Q_OBJECT
public:
    explicit ClaudeResponse(QObject *parent = nullptr);

    StreamEvent processEvent(const QJsonObject &event);

    void handleContentBlockStart(int index, const QString &blockType, const QJsonObject &data);
    void handleContentBlockDelta(int index, const QString &deltaType, const QJsonObject &delta);
    void handleContentBlockStop(int index);
    void handleStopReason(const QString &stopReason);

    QJsonObject toProviderFormat() const;
    QJsonArray createToolResultsContent(const QHash<QString, QString> &toolResults) const;

    MessageState state() const { return m_state; }
    QList<ToolUseContent *> getCurrentToolUseContent() const;
    QList<ThinkingContent *> getCurrentThinkingContent() const;
    QList<RedactedThinkingContent *> getCurrentRedactedThinkingContent() const;
    const QList<ContentBlock *> &getCurrentBlocks() const { return m_currentBlocks; }

    void startNewContinuation();

private:
    QString m_stopReason;
    MessageState m_state = MessageState::Building;
    QList<ContentBlock *> m_currentBlocks;
    QHash<int, QString> m_pendingToolInputs;

    void updateStateFromStopReason();

    template<typename T, typename... Args>
    T *addCurrentContent(Args &&...args)
    {
        T *content = new T(std::forward<Args>(args)...);
        content->setParent(this);
        m_currentBlocks.append(content);
        return content;
    }
};

} // namespace QodeAssist::LLMCore
