// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <pluginllmcore/ContentBlocks.hpp>

namespace QodeAssist::Providers {

class OpenAIResponsesMessage : public QObject
{
    Q_OBJECT
public:
    explicit OpenAIResponsesMessage(QObject *parent = nullptr);

    void handleItemDelta(const QJsonObject &item);
    void handleToolCallStart(const QString &callId, const QString &name);
    void handleToolCallDelta(const QString &callId, const QString &argumentsDelta);
    void handleToolCallComplete(const QString &callId);
    void handleReasoningStart(const QString &itemId);
    void handleReasoningDelta(const QString &itemId, const QString &text);
    void handleReasoningComplete(const QString &itemId);
    void handleStatus(const QString &status);

    QList<QJsonObject> toItemsFormat() const;
    QJsonArray createToolResultItems(const QHash<QString, QString> &toolResults) const;

    PluginLLMCore::MessageState state() const noexcept { return m_state; }
    QString accumulatedText() const;
    QList<PluginLLMCore::ToolUseContent *> getCurrentToolUseContent() const;
    QList<PluginLLMCore::ThinkingContent *> getCurrentThinkingContent() const;
    
    bool hasToolCalls() const noexcept { return !m_toolCalls.isEmpty(); }
    bool hasThinkingContent() const noexcept { return !m_thinkingBlocks.isEmpty(); }

    void startNewContinuation();

private:
    QString m_status;
    PluginLLMCore::MessageState m_state = PluginLLMCore::MessageState::Building;
    QList<PluginLLMCore::ContentBlock *> m_items;
    QHash<QString, QString> m_pendingToolArguments;
    QHash<QString, PluginLLMCore::ToolUseContent *> m_toolCalls;
    QHash<QString, PluginLLMCore::ThinkingContent *> m_thinkingBlocks;

    void updateStateFromStatus();
    PluginLLMCore::TextContent *getOrCreateTextItem();
};

} // namespace QodeAssist::Providers

