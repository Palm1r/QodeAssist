// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SessionManager.hpp"

#include "Agent.hpp"
#include "AgentFactory.hpp"
#include "Session.hpp"

namespace QodeAssist {

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{}

SessionManager::SessionManager(AgentFactory *agentFactory, QObject *parent)
    : QObject(parent)
    , m_agentFactory(agentFactory)
{}

SessionManager::~SessionManager() = default;

Session *SessionManager::createSession()
{
    auto *session = new Session(this);
    m_sessions.append(session);
    emit sessionCreated(session);
    return session;
}

Session *SessionManager::createSession(const QString &agentName, QString *errorOut)
{
    return createSession(agentName, /*externalHistory=*/nullptr, errorOut);
}

Session *SessionManager::createSession(
    const QString &agentName, ConversationHistory *externalHistory, QString *errorOut)
{
    if (!m_agentFactory) {
        if (errorOut)
            *errorOut = QStringLiteral("SessionManager: no AgentFactory bound");
        return nullptr;
    }

    QString agentErr;
    Agent *agent = m_agentFactory->create(agentName, /*parent=*/nullptr, &agentErr);
    if (!agent) {
        if (errorOut)
            *errorOut = agentErr.isEmpty()
                            ? QStringLiteral("SessionManager: agent '%1' not found").arg(agentName)
                            : agentErr;
        return nullptr;
    }

    auto *session = new Session(agent, externalHistory, this);
    if (!session->isValid()) {
        if (errorOut)
            *errorOut = session->invalidReason();
        delete session; // also deletes the reparented agent
        return nullptr;
    }

    m_sessions.append(session);
    emit sessionCreated(session);
    return session;
}

void SessionManager::removeSession(Session *session)
{
    if (!session)
        return;

    const int idx = m_sessions.indexOf(session);
    if (idx < 0)
        return;

    if (session->isInFlight())
        session->cancel();

    m_sessions.removeAt(idx);
    emit sessionRemoved(session);
    session->deleteLater();
}

QList<Session *> SessionManager::sessions() const
{
    QList<Session *> out;
    out.reserve(m_sessions.size());
    for (const auto &p : m_sessions) {
        if (p)
            out.append(p.data());
    }
    return out;
}

void SessionManager::cancelAll()
{
    const auto snapshot = m_sessions;
    for (const auto &p : snapshot) {
        if (p && p->isInFlight())
            p->cancel();
    }
}

} // namespace QodeAssist
