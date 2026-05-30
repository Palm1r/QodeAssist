// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatAgentController.hpp"

#include <QSettings>

#include <coreplugin/icore.h>

#include <AgentConfig.hpp>
#include <AgentFactory.hpp>
#include <sources/settings/PipelinesConfig.hpp>

namespace QodeAssist::Chat {

namespace {
const char kChatAgentKey[] = "QodeAssist.chatActiveAgent";
}

ChatAgentController::ChatAgentController(QObject *parent)
    : QObject(parent)
{
    if (auto *settings = Core::ICore::settings())
        m_currentAgent = settings->value(kChatAgentKey).toString();
}

void ChatAgentController::setAgentFactory(AgentFactory *factory)
{
    m_agentFactory = factory;
    if (factory)
        connect(
            factory,
            &AgentFactory::agentsChanged,
            this,
            &ChatAgentController::reload,
            Qt::UniqueConnection);
    reload();
}

QStringList ChatAgentController::availableAgents() const
{
    return m_availableAgents;
}

QString ChatAgentController::currentAgent() const
{
    return m_currentAgent;
}

void ChatAgentController::setCurrentAgent(const QString &name)
{
    if (name == m_currentAgent || !m_availableAgents.contains(name))
        return;

    applyCurrentAgent(name);
}

void ChatAgentController::reload()
{
    if (!m_agentFactory)
        return;

    const QStringList all = m_agentFactory->configNames();
    const QStringList roster = Settings::PipelinesConfig::load().rosters.chatAssistant;

    QStringList filtered;
    for (const QString &name : roster) {
        if (all.contains(name))
            filtered.append(name);
    }

    if (filtered != m_availableAgents) {
        m_availableAgents = filtered;
        emit availableAgentsChanged();
    }
    ensureValidCurrent();
}

void ChatAgentController::ensureValidCurrent()
{
    if (m_availableAgents.contains(m_currentAgent))
        return;

    const QString next = m_availableAgents.isEmpty() ? QString() : m_availableAgents.first();
    if (next == m_currentAgent)
        return;

    applyCurrentAgent(next);
}

void ChatAgentController::applyCurrentAgent(const QString &name)
{
    m_currentAgent = name;
    if (auto *settings = Core::ICore::settings())
        settings->setValue(kChatAgentKey, m_currentAgent);
    emit currentAgentChanged();
}

bool ChatAgentController::currentSupportsTools() const
{
    if (!m_agentFactory || m_currentAgent.isEmpty())
        return false;
    const AgentConfig *config = m_agentFactory->configByName(m_currentAgent);
    return config && config->enableTools;
}

} // namespace QodeAssist::Chat
