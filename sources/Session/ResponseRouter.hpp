// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <LLMQore/BaseClient.hpp>

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>

#include "ResponseEvent.hpp"

namespace QodeAssist {

class ConversationHistory;

class ResponseRouter : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ResponseRouter)
public:
    ResponseRouter(
        LLMQore::BaseClient *client,
        ConversationHistory *history,
        QObject *parent = nullptr);
    ~ResponseRouter() override;

    void beginRequest(const LLMQore::RequestID &id);
    void endRequest();

    bool isActive() const noexcept { return !m_activeId.isEmpty(); }

signals:
    void event(const QodeAssist::ResponseEvent &ev);

private slots:
    void onChunk(const LLMQore::RequestID &id, const QString &chunk);
    void onThinking(
        const LLMQore::RequestID &id, const QString &thinking, const QString &signature);
    void onToolStarted(
        const LLMQore::RequestID &id,
        const QString &toolId,
        const QString &toolName,
        const QJsonObject &arguments);
    void onToolResultReady(
        const LLMQore::RequestID &id,
        const QString &toolId,
        const QString &toolName,
        const QString &result);
    void onFinalized(const LLMQore::RequestID &id, const LLMQore::CompletionInfo &info);
    void onFailed(const LLMQore::RequestID &id, const QString &err);

private:
    void ensureAssistantOpen();
    void resetTurnState();

    QPointer<LLMQore::BaseClient> m_client;
    QPointer<ConversationHistory> m_history;

    LLMQore::RequestID m_activeId;
    bool m_assistantOpen = false;
    bool m_inToolResults = false;
};

} // namespace QodeAssist
