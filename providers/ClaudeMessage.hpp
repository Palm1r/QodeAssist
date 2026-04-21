// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <pluginllmcore/ContentBlocks.hpp>

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

    PluginLLMCore::MessageState state() const { return m_state; }
    QList<PluginLLMCore::ToolUseContent *> getCurrentToolUseContent() const;
    QList<PluginLLMCore::ThinkingContent *> getCurrentThinkingContent() const;
    QList<PluginLLMCore::RedactedThinkingContent *> getCurrentRedactedThinkingContent() const;
    const QList<PluginLLMCore::ContentBlock *> &getCurrentBlocks() const { return m_currentBlocks; }

    void startNewContinuation();

private:
    QString m_stopReason;
    PluginLLMCore::MessageState m_state = PluginLLMCore::MessageState::Building;
    QList<PluginLLMCore::ContentBlock *> m_currentBlocks;
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
