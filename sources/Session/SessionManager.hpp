// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include "ToolContributorRegistry.hpp"

namespace QodeAssist {

class AgentFactory;
class ConversationHistory;
class Session;

class SessionManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SessionManager)
public:
    explicit SessionManager(AgentFactory *agentFactory, QObject *parent = nullptr);

    ~SessionManager() override;

    Session *createSession(const QString &agentName, QString *errorOut = nullptr);

    Session *createSession(
        const QString &agentName,
        ConversationHistory *externalHistory,
        QString *errorOut = nullptr);

    void removeSession(Session *session);

    QList<Session *> sessions() const;

    void cancelAll();

    ToolContributorRegistry &toolContributors() noexcept { return m_toolContributors; }
    const ToolContributorRegistry &toolContributors() const noexcept { return m_toolContributors; }

signals:
    void sessionCreated(Session *session);
    void sessionRemoved(Session *session);

private:
    QPointer<AgentFactory> m_agentFactory;
    QList<QPointer<Session>> m_sessions;
    ToolContributorRegistry m_toolContributors;
};

} // namespace QodeAssist
