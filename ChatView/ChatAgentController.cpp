// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatAgentController.hpp"

#include <QSettings>

#include <coreplugin/icore.h>

#include <AgentConfig.hpp>
#include <AgentFactory.hpp>

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

    m_currentAgent = name;
    if (auto *settings = Core::ICore::settings())
        settings->setValue(kChatAgentKey, m_currentAgent);
    emit currentAgentChanged();
}

void ChatAgentController::reload()
{
    m_availableAgents = m_agentFactory ? m_agentFactory->configNames() : QStringList{};
    emit availableAgentsChanged();
    ensureValidCurrent();
}

void ChatAgentController::ensureValidCurrent()
{
    if (m_availableAgents.contains(m_currentAgent))
        return;

    const QString next = m_availableAgents.isEmpty() ? QString() : m_availableAgents.first();
    if (next == m_currentAgent)
        return;

    m_currentAgent = next;
    if (auto *settings = Core::ICore::settings())
        settings->setValue(kChatAgentKey, m_currentAgent);
    emit currentAgentChanged();
}

bool ChatAgentController::currentSupportsThinking() const
{
    if (!m_agentFactory || m_currentAgent.isEmpty())
        return false;
    const AgentConfig *config = m_agentFactory->configByName(m_currentAgent);
    return config && config->enableThinking;
}

bool ChatAgentController::currentSupportsTools() const
{
    if (!m_agentFactory || m_currentAgent.isEmpty())
        return false;
    const AgentConfig *config = m_agentFactory->configByName(m_currentAgent);
    return config && config->enableTools;
}

} // namespace QodeAssist::Chat
