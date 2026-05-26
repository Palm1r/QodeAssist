// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

namespace QodeAssist {

class AgentFactory;
class ConversationHistory;
class Session;

class SessionManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SessionManager)
public:
    explicit SessionManager(QObject *parent = nullptr);

    SessionManager(AgentFactory *agentFactory, QObject *parent = nullptr);

    ~SessionManager() override;

    Session *createSession();

    Session *createSession(const QString &agentName, QString *errorOut = nullptr);

    Session *createSession(
        const QString &agentName,
        ConversationHistory *externalHistory,
        QString *errorOut = nullptr);

    void removeSession(Session *session);

    QList<Session *> sessions() const;

    void cancelAll();

signals:
    void sessionCreated(Session *session);
    void sessionRemoved(Session *session);

private:
    QPointer<AgentFactory> m_agentFactory;
    QList<QPointer<Session>> m_sessions;
};

} // namespace QodeAssist
