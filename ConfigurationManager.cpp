/* 
 * Copyright (C) 2024 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ConfigurationManager.hpp"

#include <QTimer>
#include <settings/ButtonAspect.hpp>

#include "QodeAssisttr.h"

namespace QodeAssist {

ConfigurationManager &ConfigurationManager::instance()
{
    static ConfigurationManager instance;
    return instance;
}

void ConfigurationManager::init()
{
    setupConnections();
}

ConfigurationManager::ConfigurationManager(QObject *parent)
    : QObject(parent)
    , m_generalSettings(Settings::generalSettings())
    , m_providersManager(LLMCore::ProvidersManager::instance())
    , m_templateManger(LLMCore::PromptTemplateManager::instance())
{}

void ConfigurationManager::setupConnections()
{
    using Config = ConfigurationManager;
    using Button = ButtonAspect;

    connect(&m_generalSettings.ccSelectProvider, &Button::clicked, this, &Config::selectProvider);
    connect(&m_generalSettings.caSelectProvider, &Button::clicked, this, &Config::selectProvider);
    connect(&m_generalSettings.ccSelectModel, &Button::clicked, this, &Config::selectModel);
    connect(&m_generalSettings.caSelectModel, &Button::clicked, this, &Config::selectModel);
    connect(&m_generalSettings.ccSelectTemplate, &Button::clicked, this, &Config::selectTemplate);
    connect(&m_generalSettings.caSelectTemplate, &Button::clicked, this, &Config::selectTemplate);
    connect(&m_generalSettings.ccSetUrl, &Button::clicked, this, &Config::selectUrl);
    connect(&m_generalSettings.caSetUrl, &Button::clicked, this, &Config::selectUrl);
}

void ConfigurationManager::selectProvider()
{
    const auto providersList = m_providersManager.providersNames();

    auto *settingsButton = qobject_cast<ButtonAspect *>(sender());
    if (!settingsButton)
        return;

    auto &targetSettings = (settingsButton == &m_generalSettings.ccSelectProvider)
                               ? m_generalSettings.ccProvider
                               : m_generalSettings.caProvider;

    QTimer::singleShot(0, this, [this, providersList, &targetSettings] {
        m_generalSettings.showSelectionDialog(providersList,
                                              targetSettings,
                                              Tr::tr("Select LLM Provider"),
                                              Tr::tr("Providers:"));
    });
}

void ConfigurationManager::selectModel()
{
    const QString providerName = m_generalSettings.ccProvider.volatileValue();

    auto *settingsButton = qobject_cast<ButtonAspect *>(sender());
    if (!settingsButton)
        return;

    const auto providerUrl = (settingsButton == &m_generalSettings.ccSelectModel)
                                 ? m_generalSettings.ccUrl.volatileValue()
                                 : m_generalSettings.caUrl.volatileValue();
    const auto modelList = m_providersManager.getProviderByName(providerName)
                               ->getInstalledModels(providerUrl);

    auto &targetSettings = (settingsButton == &m_generalSettings.ccSelectModel)
                               ? m_generalSettings.ccModel
                               : m_generalSettings.caModel;

    QTimer::singleShot(0, &m_generalSettings, [this, modelList, &targetSettings]() {
        m_generalSettings.showSelectionDialog(modelList,
                                              targetSettings,
                                              Tr::tr("Select LLM Model"),
                                              Tr::tr("Models:"));
    });
}

void ConfigurationManager::selectTemplate()
{
    auto *settingsButton = qobject_cast<ButtonAspect *>(sender());
    if (!settingsButton)
        return;

    const auto templateList = (settingsButton == &m_generalSettings.ccSelectTemplate)
                                  ? m_templateManger.fimTemplatesNames()
                                  : m_templateManger.chatTemplatesNames();

    auto &targetSettings = (settingsButton == &m_generalSettings.ccSelectTemplate)
                               ? m_generalSettings.ccTemplate
                               : m_generalSettings.caTemplate;

    QTimer::singleShot(0, &m_generalSettings, [this, templateList, &targetSettings]() {
        m_generalSettings.showSelectionDialog(templateList,
                                              targetSettings,
                                              Tr::tr("Select Template"),
                                              Tr::tr("Templates:"));
    });
}

void ConfigurationManager::selectUrl()
{
    QStringList urls;
    for (const auto &name : m_providersManager.providersNames()) {
        const auto url = m_providersManager.getProviderByName(name)->url();
        if (!urls.contains(url))
            urls.append(url);
    }

    auto *settingsButton = qobject_cast<ButtonAspect *>(sender());
    if (!settingsButton)
        return;

    auto &targetSettings = (settingsButton == &m_generalSettings.ccSetUrl)
                               ? m_generalSettings.ccUrl
                               : m_generalSettings.caUrl;

    QTimer::singleShot(0, &m_generalSettings, [this, urls, &targetSettings]() {
        m_generalSettings.showSelectionDialog(urls,
                                              targetSettings,
                                              Tr::tr("Select URL"),
                                              Tr::tr("URLs:"));
    });
}

} // namespace QodeAssist
