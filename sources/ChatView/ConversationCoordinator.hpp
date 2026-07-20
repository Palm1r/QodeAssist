// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <optional>

#include <QObject>
#include <QString>
#include <QStringList>

#include "ConversationPorts.hpp"

namespace QodeAssist::Chat {

class ConversationCoordinator : public QObject
{
    Q_OBJECT

public:
    struct Ports
    {
        IConversationPort *ops = nullptr;
        IAgentCatalogPort *catalog = nullptr;
        ICompressionPort *compression = nullptr;
        ISendPort *send = nullptr;
    };

    explicit ConversationCoordinator(const Ports &ports, QObject *parent = nullptr);

    void requestSend(
        const QString &message, const QStringList &attachments, bool useTools, bool useThinking);
    bool refuseWhileReadOnly();
    bool hasDeferredSend() const;

    void chooseAgent(const Acp::AgentDefinition &agent);
    void chooseLlm();
    void confirmSwitch();
    void cancelSwitch();
    bool switchPending() const;

    void restoreAgentBinding(const Acp::AgentBinding &binding);
    Acp::AgentBinding bindingForSave() const;

    QString sessionIssue() const;
    bool readOnly() const;
    bool canStartFreshSession() const;

    bool canHandOverSummary() const;
    QString summaryHandoverTooltip() const;
    void startFreshSession();
    void handOverSummary();

    QString agentTitle() const;

    void conversationReset();

public slots:
    void agentTitleSuggested(const QString &title);
    void agentSessionUnavailable(const QString &reason);
    void summaryProduced(const QString &summary);
    void compressionSettled();

signals:
    void sessionIssueChanged();
    void agentTitleChanged();
    void boundToAgent(const QodeAssist::Acp::AgentDefinition &agent);
    void boundToLlm();
    void switchConfirmationNeeded(const QString &targetName);
    void switchCancelled();
    void errorSurfaced(const QString &message);

private:
    struct DeferredSend
    {
        QString message;
        QStringList attachments;
        bool useTools = false;
        bool useThinking = false;
        bool active = false;
    };

    void setSessionIssue(const QString &issue, bool recoverable);
    bool deferSendForAutoCompress(
        const QString &message, const QStringList &attachments, bool useTools, bool useThinking);
    void bindAgentNow(const Acp::AgentDefinition &agent);
    void bindLlmNow();

    Ports m_ports;
    std::optional<Acp::AgentDefinition> m_pendingAgent;
    bool m_pendingLlmSwitch = false;
    DeferredSend m_deferredSend;
    QString m_agentTitle;
    QString m_sessionIssue;
    bool m_sessionRecoverable = false;
    Acp::AgentBinding m_quarantinedBinding;
};

} // namespace QodeAssist::Chat
