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

namespace QodeAssist::Providers {

class OllamaMessage : public QObject
{
    Q_OBJECT
public:
    explicit OllamaMessage(QObject *parent = nullptr);

    void handleContentDelta(const QString &content);
    void handleToolCall(const QJsonObject &toolCall);
    void handleDone(bool done);

    QJsonObject toProviderFormat() const;
    QJsonArray createToolResultMessages(const QHash<QString, QString> &toolResults) const;

    LLMCore::MessageState state() const { return m_state; }
    QList<LLMCore::ToolUseContent *> getCurrentToolUseContent() const;
    QList<LLMCore::ContentBlock *> currentBlocks() const { return m_currentBlocks; }

    void startNewContinuation();

private:
    bool m_done = false;
    LLMCore::MessageState m_state = LLMCore::MessageState::Building;
    QList<LLMCore::ContentBlock *> m_currentBlocks;
    QString m_accumulatedContent;
    bool m_contentAddedToTextBlock = false;

    void updateStateFromDone();
    bool tryParseToolCall();
    bool isLikelyToolCallJson(const QString &content) const;
    LLMCore::TextContent *getOrCreateTextContent();

    template<typename T, typename... Args>
    T *addCurrentContent(Args &&...args)
    {
        T *content = new T(std::forward<Args>(args)...);
        content->setParent(this);
        m_currentBlocks.append(content);
        return content;
    }
};

} // namespace QodeAssist::Providers
