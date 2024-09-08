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

#include "GeneralSettings.hpp"

#include <QInputDialog>
#include <QMessageBox>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>

#include "LLMProvidersManager.hpp"
#include "PromptTemplateManager.hpp"
#include "QodeAssistConstants.hpp"
#include "QodeAssistUtils.hpp"
#include "QodeAssisttr.h"

namespace QodeAssist::Settings {

GeneralSettings &generalSettings()
{
    static GeneralSettings settings;
    return settings;
}

GeneralSettings::GeneralSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("General"));

    enableQodeAssist.setSettingsKey(Constants::ENABLE_QODE_ASSIST);
    enableQodeAssist.setLabelText(Tr::tr("Enable Qode Assist"));
    enableQodeAssist.setDefaultValue(true);

    enableAutoComplete.setSettingsKey(Constants::ENABLE_AUTO_COMPLETE);
    enableAutoComplete.setLabelText(Tr::tr("Enable Auto Complete"));
    enableAutoComplete.setDefaultValue(true);

    enableLogging.setSettingsKey(Constants::ENABLE_LOGGING);
    enableLogging.setLabelText(Tr::tr("Enable Logging"));
    enableLogging.setDefaultValue(false);

    multiLineCompletion.setSettingsKey(Constants::MULTILINE_COMPLETION);
    multiLineCompletion.setDefaultValue(true);
    multiLineCompletion.setLabelText(Tr::tr("Enable Multiline Completion"));

    startSuggestionTimer.setSettingsKey(Constants::START_SUGGESTION_TIMER);
    startSuggestionTimer.setLabelText(Tr::tr("with delay(ms)"));
    startSuggestionTimer.setRange(10, 10000);
    startSuggestionTimer.setDefaultValue(500);

    autoCompletionCharThreshold.setSettingsKey(Constants::AUTO_COMPLETION_CHAR_THRESHOLD);
    autoCompletionCharThreshold.setLabelText(Tr::tr("AI suggestion triggers after typing"));
    autoCompletionCharThreshold.setToolTip(
        Tr::tr("The number of characters that need to be typed within the typing interval "
               "before an AI suggestion request is sent."));
    autoCompletionCharThreshold.setRange(0, 10);
    autoCompletionCharThreshold.setDefaultValue(1);

    autoCompletionTypingInterval.setSettingsKey(Constants::AUTO_COMPLETION_TYPING_INTERVAL);
    autoCompletionTypingInterval.setLabelText(Tr::tr("character(s) within(ms)"));
    autoCompletionTypingInterval.setToolTip(
        Tr::tr("The time window (in milliseconds) during which the character threshold "
               "must be met to trigger an AI suggestion request."));
    autoCompletionTypingInterval.setRange(500, 5000);
    autoCompletionTypingInterval.setDefaultValue(2000);

    llmProviders.setSettingsKey(Constants::LLM_PROVIDERS);
    llmProviders.setDisplayName(Tr::tr("FIM Provider:"));
    llmProviders.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    llmProviders.setDefaultValue(0);

    url.setSettingsKey(Constants::URL);
    url.setLabelText(Tr::tr("URL:"));
    url.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    endPoint.setSettingsKey(Constants::END_POINT);
    endPoint.setLabelText(Tr::tr("FIM Endpoint:"));
    endPoint.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    modelName.setSettingsKey(Constants::MODEL_NAME);
    modelName.setLabelText(Tr::tr("LLM Name:"));
    modelName.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    selectModels.m_buttonText = Tr::tr("Select Fill-In-the-Middle Model");

    fimPrompts.setDisplayName(Tr::tr("Fill-In-the-Middle Prompt"));
    fimPrompts.setSettingsKey(Constants::FIM_PROMPTS);
    fimPrompts.setDefaultValue(0);
    fimPrompts.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    const auto &manager = LLMProvidersManager::instance();
    if (!manager.getProviderNames().isEmpty()) {
        const auto providerNames = manager.getProviderNames();
        for (const QString &name : providerNames) {
            llmProviders.addOption(name);
        }
    }

    const auto &promptManager = PromptTemplateManager::instance();
    if (!promptManager.getTemplateNames().isEmpty()) {
        const auto promptNames = promptManager.getTemplateNames();
        for (const QString &name : promptNames) {
            fimPrompts.addOption(name);
        }
    }

    readSettings();

    LLMProvidersManager::instance().setCurrentProvider(llmProviders.stringValue());
    PromptTemplateManager::instance().setCurrentTemplate(fimPrompts.stringValue());
    setLoggingEnabled(enableLogging());

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        auto rootLayout = Column{Row{enableQodeAssist, Stretch{1}, resetToDefaults},
                                 enableAutoComplete,
                                 multiLineCompletion,
                                 Row{autoCompletionCharThreshold,
                                     autoCompletionTypingInterval,
                                     startSuggestionTimer,
                                     Stretch{1}},
                                 Space{8},
                                 enableLogging,
                                 Space{8},
                                 llmProviders,
                                 Row{url, endPoint},
                                 Space{8},
                                 Row{selectModels, modelName},
                                 Space{8},
                                 fimPrompts,
                                 Stretch{1}};
        return rootLayout;
    });
}

void GeneralSettings::setupConnections()
{
    connect(&llmProviders, &Utils::SelectionAspect::volatileValueChanged, this, [this]() {
        int index = llmProviders.volatileValue();
        logMessage(QString("currentProvider %1").arg(llmProviders.displayForIndex(index)));
        LLMProvidersManager::instance().setCurrentProvider(llmProviders.displayForIndex(index));
        updateProviderSettings();
    });
    connect(&fimPrompts, &Utils::SelectionAspect::volatileValueChanged, this, [this]() {
        int index = fimPrompts.volatileValue();
        logMessage(QString("currentPrompt %1").arg(fimPrompts.displayForIndex(index)));
        PromptTemplateManager::instance().setCurrentTemplate(fimPrompts.displayForIndex(index));
    });
    connect(&selectModels, &ButtonAspect::clicked, this, [this]() { showModelSelectionDialog(); });
    connect(&enableLogging, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        setLoggingEnabled(enableLogging.volatileValue());
    });
    connect(&resetToDefaults, &ButtonAspect::clicked, this, &GeneralSettings::resetPageToDefaults);
}

void GeneralSettings::updateProviderSettings()
{
    const auto provider = LLMProvidersManager::instance().getCurrentProvider();

    if (provider) {
        url.setVolatileValue(provider->url());
        endPoint.setVolatileValue(provider->completionEndpoint());
    }
}

void GeneralSettings::showModelSelectionDialog()
{
    auto *provider = LLMProvidersManager::instance().getCurrentProvider();
    Utils::Environment env = Utils::Environment::systemEnvironment();

    if (provider) {
        QStringList models = provider->getInstalledModels(env);
        bool ok;
        QString selectedModel = QInputDialog::getItem(Core::ICore::dialogParent(),
                                                      Tr::tr("Select LLM Model"),
                                                      Tr::tr("Choose a model:"),
                                                      models,
                                                      0,
                                                      false,
                                                      &ok);

        if (ok && !selectedModel.isEmpty()) {
            modelName.setVolatileValue(selectedModel);
            writeSettings();
        }
    }
}

void GeneralSettings::resetPageToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(enableQodeAssist);
        resetAspect(enableAutoComplete);
        resetAspect(llmProviders);
        resetAspect(url);
        resetAspect(endPoint);
        resetAspect(modelName);
        resetAspect(fimPrompts);
        resetAspect(enableLogging);
        resetAspect(startSuggestionTimer);
        resetAspect(autoCompletionTypingInterval);
        resetAspect(autoCompletionCharThreshold);
    }

    fimPrompts.setStringValue("StarCoder2");
    llmProviders.setStringValue("Ollama");

    QMessageBox::information(Core::ICore::dialogParent(),
                             Tr::tr("Settings Reset"),
                             Tr::tr("All settings have been reset to their default values."));
}

class GeneralSettingsPage : public Core::IOptionsPage
{
public:
    GeneralSettingsPage()
    {
        setId(Constants::QODE_ASSIST_GENERAL_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("General"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &generalSettings(); });
    }
};

const GeneralSettingsPage generalSettingsPage;

} // namespace QodeAssist::Settings
