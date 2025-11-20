/*
 * Copyright (C) 2025 Petr Mironychev
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

#include <settings/ButtonAspect.hpp>
#include <QTimer>

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
    updateAllTemplateDescriptions();
    checkAllTemplate();
}

void ConfigurationManager::updateTemplateDescription(const Utils::StringAspect &templateAspect)
{
    LLMCore::PromptTemplate *templ = m_templateManger.getFimTemplateByName(templateAspect.value());

    if (!templ) {
        return;
    }

    if (&templateAspect == &m_generalSettings.ccTemplate) {
        m_generalSettings.ccTemplateDescription.setValue(templ->description());
    } else if (&templateAspect == &m_generalSettings.caTemplate) {
        m_generalSettings.caTemplateDescription.setValue(templ->description());
    } else if (&templateAspect == &m_generalSettings.qrTemplate) {
        m_generalSettings.qrTemplateDescription.setValue(templ->description());
    }
}

void ConfigurationManager::updateAllTemplateDescriptions()
{
    updateTemplateDescription(m_generalSettings.ccTemplate);
    updateTemplateDescription(m_generalSettings.caTemplate);
    updateTemplateDescription(m_generalSettings.qrTemplate);
}

void ConfigurationManager::checkTemplate(const Utils::StringAspect &templateAspect)
{
    LLMCore::PromptTemplate *templ = m_templateManger.getFimTemplateByName(templateAspect.value());

    if (templ->name() == templateAspect.value())
        return;

    if (&templateAspect == &m_generalSettings.ccTemplate) {
        m_generalSettings.ccTemplate.setValue(templ->name());
    } else if (&templateAspect == &m_generalSettings.caTemplate) {
        m_generalSettings.caTemplate.setValue(templ->name());
    }
}

void ConfigurationManager::checkAllTemplate()
{
    checkTemplate(m_generalSettings.ccTemplate);
    checkTemplate(m_generalSettings.caTemplate);
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
    connect(&m_generalSettings.qrSelectProvider, &Button::clicked, this, &Config::selectProvider);
    connect(&m_generalSettings.ccSelectModel, &Button::clicked, this, &Config::selectModel);
    connect(&m_generalSettings.caSelectModel, &Button::clicked, this, &Config::selectModel);
    connect(&m_generalSettings.qrSelectModel, &Button::clicked, this, &Config::selectModel);
    connect(&m_generalSettings.ccSelectTemplate, &Button::clicked, this, &Config::selectTemplate);
    connect(&m_generalSettings.caSelectTemplate, &Button::clicked, this, &Config::selectTemplate);
    connect(&m_generalSettings.qrSelectTemplate, &Button::clicked, this, &Config::selectTemplate);
    connect(&m_generalSettings.ccSetUrl, &Button::clicked, this, &Config::selectUrl);
    connect(&m_generalSettings.caSetUrl, &Button::clicked, this, &Config::selectUrl);
    connect(&m_generalSettings.qrSetUrl, &Button::clicked, this, &Config::selectUrl);

    connect(
        &m_generalSettings.ccPreset1SelectProvider, &Button::clicked, this, &Config::selectProvider);
    connect(&m_generalSettings.ccPreset1SetUrl, &Button::clicked, this, &Config::selectUrl);
    connect(&m_generalSettings.ccPreset1SelectModel, &Button::clicked, this, &Config::selectModel);
    connect(
        &m_generalSettings.ccPreset1SelectTemplate, &Button::clicked, this, &Config::selectTemplate);

    connect(&m_generalSettings.ccTemplate, &Utils::StringAspect::changed, this, [this]() {
        updateTemplateDescription(m_generalSettings.ccTemplate);
    });

    connect(&m_generalSettings.caTemplate, &Utils::StringAspect::changed, this, [this]() {
        updateTemplateDescription(m_generalSettings.caTemplate);
    });

    connect(&m_generalSettings.qrTemplate, &Utils::StringAspect::changed, this, [this]() {
        updateTemplateDescription(m_generalSettings.qrTemplate);
    });
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
                           : settingsButton == &m_generalSettings.qrSelectProvider
                               ? m_generalSettings.qrProvider
                               : m_generalSettings.caProvider;

    QTimer::singleShot(0, this, [this, providersList, &targetSettings] {
        m_generalSettings.showSelectionDialog(
            providersList, targetSettings, Tr::tr("Select LLM Provider"), Tr::tr("Providers:"));
    });
}

void ConfigurationManager::selectModel()
{
    auto *settingsButton = qobject_cast<ButtonAspect *>(sender());
    if (!settingsButton)
        return;

    const bool isCodeCompletion = (settingsButton == &m_generalSettings.ccSelectModel);
    const bool isPreset1 = (settingsButton == &m_generalSettings.ccPreset1SelectModel);
    const bool isQuickRefactor = (settingsButton == &m_generalSettings.qrSelectModel);

    const QString providerName = isCodeCompletion ? m_generalSettings.ccProvider.volatileValue()
                                 : isPreset1 ? m_generalSettings.ccPreset1Provider.volatileValue()
                                 : isQuickRefactor ? m_generalSettings.qrProvider.volatileValue()
                                             : m_generalSettings.caProvider.volatileValue();

    const auto providerUrl = isCodeCompletion ? m_generalSettings.ccUrl.volatileValue()
                             : isPreset1      ? m_generalSettings.ccPreset1Url.volatileValue()
                             : isQuickRefactor ? m_generalSettings.qrUrl.volatileValue()
                                              : m_generalSettings.caUrl.volatileValue();

    auto &targetSettings = isCodeCompletion ? m_generalSettings.ccModel
                           : isPreset1      ? m_generalSettings.ccPreset1Model
                           : isQuickRefactor ? m_generalSettings.qrModel
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
    const bool isQuickRefactor = (settingsButton == &m_generalSettings.qrSelectTemplate);
    const QString providerName = isCodeCompletion ? m_generalSettings.ccProvider.volatileValue()
                                 : isPreset1 ? m_generalSettings.ccPreset1Provider.volatileValue()
                                 : isQuickRefactor ? m_generalSettings.qrProvider.volatileValue()
                                             : m_generalSettings.caProvider.volatileValue();
    auto providerID = m_providersManager.getProviderByName(providerName)->providerID();

    const auto templateList = isCodeCompletion || isPreset1
                                  ? m_templateManger.getFimTemplatesForProvider(providerID)
                                  : m_templateManger.getChatTemplatesForProvider(providerID);

    auto &targetSettings = isCodeCompletion ? m_generalSettings.ccTemplate
                           : isPreset1      ? m_generalSettings.ccPreset1Template
                           : isQuickRefactor ? m_generalSettings.qrTemplate
                                            : m_generalSettings.caTemplate;

    QTimer::singleShot(0, &m_generalSettings, [this, templateList, &targetSettings]() {
        m_generalSettings.showSelectionDialog(
            templateList, targetSettings, Tr::tr("Select Template"), Tr::tr("Templates:"));
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
                           : settingsButton == &m_generalSettings.qrSetUrl
                               ? m_generalSettings.qrUrl
                               : m_generalSettings.caUrl;

    QTimer::singleShot(0, &m_generalSettings, [this, urls, &targetSettings]() {
        m_generalSettings.showUrlSelectionDialog(targetSettings, urls);
    });
}

} // namespace QodeAssist
