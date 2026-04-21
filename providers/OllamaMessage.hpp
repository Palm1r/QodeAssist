// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <pluginllmcore/ContentBlocks.hpp>

namespace QodeAssist::Providers {

class OllamaMessage : public QObject
{
    Q_OBJECT
public:
    explicit OllamaMessage(QObject *parent = nullptr);

    void handleContentDelta(const QString &content);
    void handleToolCall(const QJsonObject &toolCall);
    void handleThinkingDelta(const QString &thinking);
    void handleThinkingComplete(const QString &signature);
    void handleDone(bool done);

    QJsonObject toProviderFormat() const;
    QJsonArray createToolResultMessages(const QHash<QString, QString> &toolResults) const;

    PluginLLMCore::MessageState state() const { return m_state; }
    QList<PluginLLMCore::ToolUseContent *> getCurrentToolUseContent() const;
    QList<PluginLLMCore::ThinkingContent *> getCurrentThinkingContent() const;
    QList<PluginLLMCore::ContentBlock *> currentBlocks() const { return m_currentBlocks; }

    void startNewContinuation();

private:
    bool m_done = false;
    PluginLLMCore::MessageState m_state = PluginLLMCore::MessageState::Building;
    QList<PluginLLMCore::ContentBlock *> m_currentBlocks;
    QString m_accumulatedContent;
    bool m_contentAddedToTextBlock = false;
    PluginLLMCore::ThinkingContent *m_currentThinkingContent = nullptr;

    void updateStateFromDone();
    bool tryParseToolCall();
    bool isLikelyToolCallJson(const QString &content) const;
    PluginLLMCore::TextContent *getOrCreateTextContent();
    PluginLLMCore::ThinkingContent *getOrCreateThinkingContent();

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
