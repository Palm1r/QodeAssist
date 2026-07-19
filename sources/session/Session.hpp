// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <optional>

#include "session/ChatBackend.hpp"
#include "session/ConversationHistory.hpp"
#include "session/HistoryProjection.hpp"
#include "session/SessionEvent.hpp"
#include "session/TurnContext.hpp"
#include "session/TurnRequest.hpp"

namespace QodeAssist::Session {

class Session : public QObject
{
    Q_OBJECT

public:
    explicit Session(QObject *parent = nullptr);

    void setBackend(ChatBackend *backend);

    const ConversationHistory &history() const;
    const QList<MessageRow> &rows() const;

    void setHistory(const ConversationHistory &history);
    void clear();
    void truncateRows(int rowIndex);

    void sendTurn(
        const QList<ContentBlock> &userBlocks,
        const std::optional<TurnContext> &context,
        const TurnOptions &options);
    void cancel();

    bool updateFileEditStatus(
        const QString &editId, const QString &status, const QString &statusMessage = {});

signals:
    void rowsAppended(const QList<QodeAssist::Session::MessageRow> &rows);
    void rowUpdated(int index, const QodeAssist::Session::MessageRow &row);
    void rowsRemoved(int first, int count);
    void rowsReset(const QList<QodeAssist::Session::MessageRow> &rows);
    void turnStarted(const QString &turnId);
    void turnFinished(const QString &turnId);
    void turnFailed(const QString &error);
    void usageReceived(const QodeAssist::Session::Usage &usage);

private:
    void handleEvent(const SessionEvent &event);
    void appendMessage(const Message &message);
    Message *activeAssistantMessage();
    void mutateAssistant(const std::function<void(Message &)> &mutate);
    void mutateAssistantTail(const std::function<void(Message &)> &mutate);
    void ensureAssistantMessage();
    void updateLastAssistantRow();
    void syncAssistantRows();
    void endTurn();

    ConversationHistory m_history;
    QList<MessageRow> m_rows;
    QPointer<ChatBackend> m_backend;
    QString m_activeTurnId;
    QString m_textSegment;
    int m_assistantRowStart = -1;
};

} // namespace QodeAssist::Session
