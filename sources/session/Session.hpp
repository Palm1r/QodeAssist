// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
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

    void respondPermission(const QString &requestId, const QString &optionId);
    bool isPermissionPending(const QString &requestId) const;

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
    void applyAgentToolCall(const ToolCallUpdated &update);
    void applyAgentPlan(const PlanUpdated &plan);
    bool refreshAssistantBlockRow(const ContentBlock &block, RowKind kind, const QString &rowId);
    void applyPermissionRequest(const PermissionRequested &request);
    void applyPermissionResolution(const PermissionResolved &resolution);
    void mutatePermissionBlock(
        const QString &requestId, const std::function<void(PermissionBlock &)> &mutate);
    std::optional<PermissionBlock> permissionBlock(const QString &requestId) const;
    QString autoAnswerOptionFor(const PermissionRequested &request) const;
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
    QSet<QString> m_pendingPermissions;
    QSet<QString> m_alwaysAllowedToolKinds;
    QSet<QString> m_alwaysRejectedToolKinds;
};

} // namespace QodeAssist::Session
