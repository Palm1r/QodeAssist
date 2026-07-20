// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatConfigurationController.hpp"

#include <utils/aspects.h>

#include "ConfigurationManager.hpp"
#include "GeneralSettings.hpp"
#include "acp/AgentCatalogStore.hpp"

namespace QodeAssist::Chat {

namespace {

QString agentEntry(const QString &agentName)
{
    return ChatConfigurationController::tr("Agent: %1").arg(agentName);
}

} // namespace

ChatConfigurationController::ChatConfigurationController(QObject *parent)
    : QObject(parent)
    , m_agents(new Acp::AgentCatalogStore(this))
{
    auto &settings = Settings::generalSettings();
    connect(
        &settings.caProvider,
        &Utils::BaseAspect::changed,
        this,
        &ChatConfigurationController::updateCurrentConfiguration);
    connect(
        &settings.caModel,
        &Utils::BaseAspect::changed,
        this,
        &ChatConfigurationController::updateCurrentConfiguration);

    loadAvailableConfigurations();
}

QStringList ChatConfigurationController::availableConfigurations() const
{
    return m_availableConfigurations;
}

QString ChatConfigurationController::currentConfiguration() const
{
    return m_currentConfiguration;
}

void ChatConfigurationController::updateCurrentConfiguration()
{
    if (!m_boundAgentName.isEmpty()) {
        m_currentConfiguration = agentEntry(m_boundAgentName);
        emit currentConfigurationChanged();
        return;
    }

    auto &settings = Settings::generalSettings();
    m_currentConfiguration
        = QString("%1 - %2").arg(settings.caProvider.value(), settings.caModel.value());
    emit currentConfigurationChanged();
}

void ChatConfigurationController::setBoundAgent(const Acp::AgentDefinition &agent)
{
    m_boundAgentName = agent.name;
    updateCurrentConfiguration();
}

void ChatConfigurationController::clearBoundAgent()
{
    if (m_boundAgentName.isEmpty())
        return;

    m_boundAgentName.clear();
    updateCurrentConfiguration();
}

void ChatConfigurationController::loadAvailableConfigurations()
{
    auto &manager = Settings::ConfigurationManager::instance();
    manager.loadConfigurations(Settings::ConfigurationType::Chat);

    QVector<Settings::AIConfiguration> configs = manager.configurations(
        Settings::ConfigurationType::Chat);

    m_availableConfigurations.clear();
    m_agentIdByEntry.clear();
    m_availableConfigurations.append(QObject::tr("Current Settings"));

    for (const Settings::AIConfiguration &config : configs) {
        m_availableConfigurations.append(config.name);
    }

    m_agents->reload();
    for (const Acp::AgentDefinition &agent : m_agents->catalog().launchableAgents()) {
        const QString entry = agentEntry(agent.name);
        m_agentIdByEntry.insert(entry, agent.id);
        m_availableConfigurations.append(entry);
    }

    updateCurrentConfiguration();

    emit availableConfigurationsChanged();
}

std::optional<Acp::AgentDefinition> ChatConfigurationController::agentById(const QString &agentId)
{
    if (agentId.isEmpty())
        return std::nullopt;

    if (auto agent = m_agents->catalog().agent(agentId))
        return agent;

    m_agents->reload();
    return m_agents->catalog().agent(agentId);
}

void ChatConfigurationController::applyConfiguration(const QString &configName)
{
    const QString agentId = m_agentIdByEntry.value(configName);
    if (!agentId.isEmpty()) {
        if (const auto agent = m_agents->catalog().agent(agentId))
            emit agentRequested(*agent);
        return;
    }

    if (configName == QObject::tr("Current Settings")) {
        emit llmRequested();
        return;
    }

    emit llmRequested();

    auto &manager = Settings::ConfigurationManager::instance();
    QVector<Settings::AIConfiguration> configs = manager.configurations(
        Settings::ConfigurationType::Chat);

    for (const Settings::AIConfiguration &config : configs) {
        if (config.name == configName) {
            auto &settings = Settings::generalSettings();

            settings.caProvider.setValue(config.provider);
            settings.caModel.setValue(config.model);
            settings.caTemplate.setValue(config.templateName);
            settings.caUrl.setValue(config.url);
            settings.caCustomEndpoint.setValue(config.customEndpoint);

            settings.writeSettings();

            m_currentConfiguration = QString("%1 - %2").arg(config.provider, config.model);
            emit currentConfigurationChanged();

            break;
        }
    }
}

} // namespace QodeAssist::Chat
