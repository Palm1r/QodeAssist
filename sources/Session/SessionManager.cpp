// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "SessionManager.hpp"

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ToolsManager.hpp>

#include "Agent.hpp"
#include "AgentFactory.hpp"
#include "ConversationHistory.hpp"
#include "Session.hpp"
#include "SystemPromptBuilder.hpp"

namespace QodeAssist {

SessionManager::SessionManager(AgentFactory *agentFactory, QObject *parent)
    : QObject(parent)
    , m_agentFactory(agentFactory)
{
    if (m_agentFactory)
        connect(m_agentFactory, &AgentFactory::agentsChanged, this, &SessionManager::flushPool);
}

SessionManager::~SessionManager() = default;

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

Session *SessionManager::createDetachedSession(ConversationHistory *externalHistory, QObject *parent)
{
    return new Session(/*agent=*/nullptr, externalHistory, parent);
}

bool SessionManager::rebindAgentByName(Session *session, const QString &agentName, QString *errorOut)
{
    if (!session) {
        if (errorOut)
            *errorOut = QStringLiteral("SessionManager: null session");
        return false;
    }
    if (!m_agentFactory) {
        if (errorOut)
            *errorOut = QStringLiteral("SessionManager: no AgentFactory bound");
        return false;
    }

    QString agentErr;
    Agent *agent = m_agentFactory->create(agentName, /*parent=*/nullptr, &agentErr);
    if (!agent) {
        if (errorOut)
            *errorOut = agentErr.isEmpty()
                            ? QStringLiteral("SessionManager: agent '%1' not found").arg(agentName)
                            : agentErr;
        return false;
    }

    session->setAgent(agent);
    if (!session->isValid()) {
        if (errorOut)
            *errorOut = session->invalidReason();
        return false;
    }
    return true;
}

Session *SessionManager::acquire(const QString &agentName, QString *errorOut)
{
    auto &bucket = m_pool[agentName];
    while (!bucket.isEmpty()) {
        QPointer<Session> pooled = bucket.takeLast();
        if (pooled && pooled->isValid() && pooledAgentMatchesCurrent(pooled, agentName)) {
            resetSession(pooled);
            m_sessions.append(pooled);
            return pooled.data();
        }
        if (pooled)
            pooled->deleteLater();
    }

    return createSession(agentName, /*externalHistory=*/nullptr, errorOut);
}

void SessionManager::release(Session *session)
{
    if (!session)
        return;

    const int idx = m_sessions.indexOf(session);
    if (idx < 0)
        return;
    m_sessions.removeAt(idx);

    if (session->isInFlight())
        session->cancel();

    session->disconnect();
    resetSession(session);

    const QString agentName
        = session->agent() ? session->agent()->config().name : QString();
    QList<QPointer<Session>> &bucket = m_pool[agentName];
    if (agentName.isEmpty() || bucket.size() >= kMaxPooledPerAgent) {
        emit sessionRemoved(session);
        session->deleteLater();
    } else {
        bucket.append(session);
    }
}

void SessionManager::resetSession(Session *session)
{
    if (!session)
        return;
    if (auto *history = session->history())
        history->clear();
    if (auto *systemPrompt = session->systemPrompt())
        systemPrompt->clear();
    if (auto *client = session->client()) {
        if (auto *tools = client->tools())
            tools->removeAllTools();
    }
}

bool SessionManager::pooledAgentMatchesCurrent(Session *session, const QString &agentName) const
{
    if (!m_agentFactory)
        return true;
    Agent *agent = session ? session->agent() : nullptr;
    if (!agent)
        return false;
    const AgentConfig *current = m_agentFactory->configByName(agentName);
    if (!current)
        return false;
    const AgentConfig &snapshot = agent->config();
    return snapshot.model == current->model
           && snapshot.providerInstance == current->providerInstance;
}

void SessionManager::flushPool()
{
    for (auto &bucket : m_pool) {
        for (const QPointer<Session> &pooled : bucket) {
            if (pooled)
                pooled->deleteLater();
        }
    }
    m_pool.clear();
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

} // namespace QodeAssist
