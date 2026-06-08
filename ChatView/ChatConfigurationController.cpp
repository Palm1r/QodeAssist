// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatConfigurationController.hpp"

#include <utils/aspects.h>

#include "ConfigurationManager.hpp"
#include "GeneralSettings.hpp"

namespace QodeAssist::Chat {

ChatConfigurationController::ChatConfigurationController(QObject *parent)
    : QObject(parent)
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
    auto &settings = Settings::generalSettings();
    m_currentConfiguration
        = QString("%1 - %2").arg(settings.caProvider.value(), settings.caModel.value());
    emit currentConfigurationChanged();
}

void ChatConfigurationController::loadAvailableConfigurations()
{
    auto &manager = Settings::ConfigurationManager::instance();
    manager.loadConfigurations(Settings::ConfigurationType::Chat);

    QVector<Settings::AIConfiguration> configs = manager.configurations(
        Settings::ConfigurationType::Chat);

    m_availableConfigurations.clear();
    m_availableConfigurations.append(QObject::tr("Current Settings"));

    for (const Settings::AIConfiguration &config : configs) {
        m_availableConfigurations.append(config.name);
    }

    updateCurrentConfiguration();

    emit availableConfigurationsChanged();
}

void ChatConfigurationController::applyConfiguration(const QString &configName)
{
    if (configName == QObject::tr("Current Settings")) {
        return;
    }

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
