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

namespace QodeAssist {

class ClaudeMessage : public QObject
{
    Q_OBJECT
public:
    explicit ClaudeMessage(QObject *parent = nullptr);

    void handleContentBlockStart(int index, const QString &blockType, const QJsonObject &data);
    void handleContentBlockDelta(int index, const QString &deltaType, const QJsonObject &delta);
    void handleContentBlockStop(int index);
    void handleStopReason(const QString &stopReason);

    QJsonObject toProviderFormat() const;
    QJsonArray createToolResultsContent(const QHash<QString, QString> &toolResults) const;

    LLMCore::MessageState state() const { return m_state; }
    QList<LLMCore::ToolUseContent *> getCurrentToolUseContent() const;

    void startNewContinuation();

private:
    QString m_stopReason;
    LLMCore::MessageState m_state = LLMCore::MessageState::Building;
    QList<LLMCore::ContentBlock *> m_currentBlocks;
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

} // namespace QodeAssist
