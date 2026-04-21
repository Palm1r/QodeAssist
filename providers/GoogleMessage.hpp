// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

#include <pluginllmcore/ContentBlocks.hpp>

namespace QodeAssist::Providers {

class GoogleMessage : public QObject
{
    Q_OBJECT
public:
    explicit GoogleMessage(QObject *parent = nullptr);

    void handleContentDelta(const QString &text);
    void handleThoughtDelta(const QString &text);
    void handleThoughtSignature(const QString &signature);
    void handleFunctionCallStart(const QString &name);
    void handleFunctionCallArgsDelta(const QString &argsJson);
    void handleFunctionCallComplete();
    void handleFinishReason(const QString &reason);

    QJsonObject toProviderFormat() const;
    QJsonArray createToolResultParts(const QHash<QString, QString> &toolResults) const;

    QList<PluginLLMCore::ToolUseContent *> getCurrentToolUseContent() const;
    QList<PluginLLMCore::ThinkingContent *> getCurrentThinkingContent() const;
    QList<PluginLLMCore::ContentBlock *> currentBlocks() const { return m_currentBlocks; }

    PluginLLMCore::MessageState state() const { return m_state; }
    QString finishReason() const { return m_finishReason; }
    bool isErrorFinishReason() const;
    QString getErrorMessage() const;
    void startNewContinuation();

private:
    void updateStateFromFinishReason();

    QList<PluginLLMCore::ContentBlock *> m_currentBlocks;
    QString m_pendingFunctionArgs;
    QString m_currentFunctionName;
    QString m_finishReason;
    PluginLLMCore::MessageState m_state = PluginLLMCore::MessageState::Building;
};

} // namespace QodeAssist::Providers
