// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <pluginllmcore/ContentBlocks.hpp>

namespace QodeAssist::Providers {

class OpenAIMessage : public QObject
{
    Q_OBJECT
public:
    explicit OpenAIMessage(QObject *parent = nullptr);

    void handleContentDelta(const QString &content);
    void handleToolCallStart(int index, const QString &id, const QString &name);
    void handleToolCallDelta(int index, const QString &argumentsDelta);
    void handleToolCallComplete(int index);
    void handleFinishReason(const QString &finishReason);

    QJsonObject toProviderFormat() const;
    QJsonArray createToolResultMessages(const QHash<QString, QString> &toolResults) const;

    PluginLLMCore::MessageState state() const { return m_state; }
    QList<PluginLLMCore::ToolUseContent *> getCurrentToolUseContent() const;

    void startNewContinuation();

private:
    QString m_finishReason;
    PluginLLMCore::MessageState m_state = PluginLLMCore::MessageState::Building;
    QList<PluginLLMCore::ContentBlock *> m_currentBlocks;
    QHash<int, QString> m_pendingToolArguments;

    void updateStateFromFinishReason();
    PluginLLMCore::TextContent *getOrCreateTextContent();

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
