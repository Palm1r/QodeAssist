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

    connect(
        &m_generalSettings.ccPreset1SelectProvider, &Button::clicked, this, &Config::selectProvider);
    connect(&m_generalSettings.ccPreset1SetUrl, &Button::clicked, this, &Config::selectUrl);
    connect(&m_generalSettings.ccPreset1SelectModel, &Button::clicked, this, &Config::selectModel);
    connect(
        &m_generalSettings.ccPreset1SelectTemplate, &Button::clicked, this, &Config::selectTemplate);
}

void ConfigurationManager::selectProvider()
{
    const auto providersList = m_providersManager.providersNames();

    auto *settingsButton = qobject_cast<ButtonAspect *>(sender());
    if (!settingsButton)
        return;

    auto &targetSettings = (settingsButton == &m_generalSettings.ccSelectProvider)
                               ? m_generalSettings.ccProvider
                           : settingsButton == &m_generalSettings.ccPreset1SelectProvider
                               ? m_generalSettings.ccPreset1Provider
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
    auto *settingsButton = qobject_cast<ButtonAspect *>(sender());
    if (!settingsButton)
        return;

    const bool isCodeCompletion = (settingsButton == &m_generalSettings.ccSelectModel);
    const bool isPreset1 = (settingsButton == &m_generalSettings.ccPreset1SelectModel);

    const QString providerName = isCodeCompletion ? m_generalSettings.ccProvider.volatileValue()
                                 : isPreset1 ? m_generalSettings.ccPreset1Provider.volatileValue()
                                             : m_generalSettings.caProvider.volatileValue();

    const auto providerUrl = isCodeCompletion ? m_generalSettings.ccUrl.volatileValue()
                             : isPreset1      ? m_generalSettings.ccPreset1Url.volatileValue()
                                              : m_generalSettings.caUrl.volatileValue();

    auto &targetSettings = isCodeCompletion ? m_generalSettings.ccModel
                           : isPreset1      ? m_generalSettings.ccPreset1Model
                                            : m_generalSettings.caModel;

    if (auto provider = m_providersManager.getProviderByName(providerName)) {
        if (!provider->supportsModelListing()) {
            m_generalSettings.showModelsNotSupportedDialog(targetSettings);
            return;
        }

        const auto modelList = provider->getInstalledModels(providerUrl);

        if (modelList.isEmpty()) {
            m_generalSettings.showModelsNotFoundDialog(targetSettings);
            return;
        }

        QTimer::singleShot(0, &m_generalSettings, [this, modelList, &targetSettings]() {
            m_generalSettings.showSelectionDialog(
                modelList, targetSettings, Tr::tr("Select LLM Model"), Tr::tr("Models:"));
        });
    }
}

void ConfigurationManager::selectTemplate()
{
    auto *settingsButton = qobject_cast<ButtonAspect *>(sender());
    if (!settingsButton)
        return;

    const bool isCodeCompletion = (settingsButton == &m_generalSettings.ccSelectTemplate);
    const bool isPreset1 = (settingsButton == &m_generalSettings.ccPreset1SelectTemplate);
    const QString providerName = isCodeCompletion ? m_generalSettings.ccProvider.volatileValue()
                                 : isPreset1 ? m_generalSettings.ccPreset1Provider.volatileValue()
                                             : m_generalSettings.caProvider.volatileValue();
    auto providerID = m_providersManager.getProviderByName(providerName)->providerID();

    const auto templateList = isCodeCompletion || isPreset1
                                  ? m_templateManger.getFimTemplatesForProvider(providerID)
                                  : m_templateManger.getChatTemplatesForProvider(providerID);

    auto &targetSettings = isCodeCompletion ? m_generalSettings.ccTemplate
                           : isPreset1      ? m_generalSettings.ccPreset1Template
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
    auto *settingsButton = qobject_cast<ButtonAspect *>(sender());
    if (!settingsButton)
        return;

    QStringList urls;
    for (const auto &name : m_providersManager.providersNames()) {
        const auto url = m_providersManager.getProviderByName(name)->url();
        if (!urls.contains(url))
            urls.append(url);
    }

    auto &targetSettings = (settingsButton == &m_generalSettings.ccSetUrl) ? m_generalSettings.ccUrl
                           : settingsButton == &m_generalSettings.ccPreset1SetUrl
                               ? m_generalSettings.ccPreset1Url
                               : m_generalSettings.caUrl;

    QTimer::singleShot(0, &m_generalSettings, [this, urls, &targetSettings]() {
        m_generalSettings.showUrlSelectionDialog(targetSettings, urls);
    });
}

} // namespace QodeAssist
