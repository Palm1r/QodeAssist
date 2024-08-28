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

#include "QodeAssistSettings.hpp"

#include <QInputDialog>
#include <QtWidgets/qmessagebox.h>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>

#include "QodeAssistConstants.hpp"
#include "QodeAssisttr.h"

#include "LLMProvidersManager.hpp"
#include "PromptTemplateManager.hpp"
#include "QodeAssistUtils.hpp"

namespace QodeAssist {

QodeAssistSettings &settings()
{
    static QodeAssistSettings settings;
    return settings;
}

QodeAssistSettings::QodeAssistSettings()
{
    setAutoApply(false);

    enableQodeAssist.setSettingsKey(Constants::ENABLE_QODE_ASSIST);
    enableQodeAssist.setLabelText(Tr::tr("Enable Qode Assist"));
    enableQodeAssist.setDefaultValue(true);

    enableAutoComplete.setSettingsKey(Constants::ENABLE_AUTO_COMPLETE);
    enableAutoComplete.setLabelText(Tr::tr("Enable Auto Complete"));
    enableAutoComplete.setDefaultValue(true);

    enableLogging.setSettingsKey(Constants::ENABLE_LOGGING);
    enableLogging.setLabelText(Tr::tr("Enable Logging"));
    enableLogging.setDefaultValue(false);

    llmProviders.setSettingsKey(Constants::LLM_PROVIDERS);
    llmProviders.setDisplayName(Tr::tr("LLM Providers:"));
    llmProviders.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    llmProviders.setDefaultValue(1);

    url.setSettingsKey(Constants::URL);
    url.setLabelText(Tr::tr("URL:"));
    url.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    endPoint.setSettingsKey(Constants::END_POINT);
    endPoint.setLabelText(Tr::tr("Endpoint:"));
    endPoint.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    port.setSettingsKey(Constants::PORT);
    port.setLabelText(Tr::tr("Port:"));
    port.setRange(1, 65535);

    modelName.setSettingsKey(Constants::MODEL_NAME);
    modelName.setLabelText(Tr::tr("LLM Name:"));
    modelName.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    temperature.setSettingsKey(Constants::TEMPERATURE);
    temperature.setLabelText(Tr::tr("Temperature:"));
    temperature.setDefaultValue(0.2);
    temperature.setRange(0.0, 10.0);

    selectModels.m_buttonText = Tr::tr("Select Models");

    ollamaLivetime.setSettingsKey(Constants::OLLAMA_LIVETIME);
    ollamaLivetime.setLabelText(
        Tr::tr("Time to suspend Ollama after completion request (in minutes), "
               "Only Ollama,  -1 to disable"));
    ollamaLivetime.setDefaultValue("5m");
    ollamaLivetime.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    fimPrompts.setDisplayName(Tr::tr("Fill-In-Middle Prompt"));
    fimPrompts.setSettingsKey(Constants::FIM_PROMPTS);
    fimPrompts.setDefaultValue(1);
    fimPrompts.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);

    readFullFile.setSettingsKey(Constants::READ_FULL_FILE);
    readFullFile.setLabelText(Tr::tr("Read Full File"));
    readFullFile.setDefaultValue(false);

    maxFileThreshold.setSettingsKey(Constants::MAX_FILE_THRESHOLD);
    maxFileThreshold.setLabelText(Tr::tr("Max File Threshold:"));
    maxFileThreshold.setRange(10, 100000);
    maxFileThreshold.setDefaultValue(600);

    readStringsBeforeCursor.setSettingsKey(Constants::READ_STRINGS_BEFORE_CURSOR);
    readStringsBeforeCursor.setLabelText(Tr::tr("Read Strings Before Cursor"));
    readStringsBeforeCursor.setDefaultValue(50);

    readStringsAfterCursor.setSettingsKey(Constants::READ_STRINGS_AFTER_CURSOR);
    readStringsAfterCursor.setLabelText(Tr::tr("Read Strings After Cursor"));
    readStringsAfterCursor.setDefaultValue(30);

    maxTokens.setSettingsKey(Constants::MAX_TOKENS);
    maxTokens.setLabelText(Tr::tr("Max Tokens"));
    maxTokens.setRange(-1, 10000);
    maxTokens.setDefaultValue(250);

    useTopP.setSettingsKey(Constants::USE_TOP_P);
    useTopP.setDefaultValue(false);

    topP.setSettingsKey(Constants::TOP_P);
    topP.setLabelText(Tr::tr("top_p"));
    topP.setDefaultValue(0.9);
    topP.setRange(0.0, 10.0);

    useTopK.setSettingsKey(Constants::USE_TOP_K);
    useTopK.setDefaultValue(false);

    topK.setSettingsKey(Constants::TOP_K);
    topK.setLabelText(Tr::tr("top_k"));
    topK.setDefaultValue(0.1);
    topK.setRange(0, 10.0);

    usePresencePenalty.setSettingsKey(Constants::USE_PRESENCE_PENALTY);
    usePresencePenalty.setDefaultValue(false);

    presencePenalty.setSettingsKey(Constants::PRESENCE_PENALTY);
    presencePenalty.setLabelText(Tr::tr("presence_penalty"));
    presencePenalty.setDefaultValue(0.0);
    presencePenalty.setRange(-2.0, 2.0);

    useFrequencyPenalty.setSettingsKey(Constants::USE_FREQUENCY_PENALTY);
    useFrequencyPenalty.setDefaultValue(false);

    frequencyPenalty.setSettingsKey(Constants::FREQUENCY_PENALTY);
    frequencyPenalty.setLabelText(Tr::tr("frequency_penalty"));
    frequencyPenalty.setDefaultValue(0.0);
    frequencyPenalty.setRange(-2.0, 2.0);

    providerPaths.setSettingsKey(Constants::PROVIDER_PATHS);
    providerPaths.setLabelText(Tr::tr("Provider Paths:"));

    startSuggestionTimer.setSettingsKey(Constants::START_SUGGESTION_TIMER);
    startSuggestionTimer.setLabelText(Tr::tr("Start Suggestion Timer:"));
    startSuggestionTimer.setRange(10, 10000);
    startSuggestionTimer.setDefaultValue(500);

    specificInstractions.setSettingsKey(Constants::SPECIFIC_INSTRUCTIONS);
    specificInstractions.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    specificInstractions.setLabelText(
        Tr::tr("Instructions: Please keep %1 for languge name, warning, it shouldn't too big"));
    specificInstractions.setDefaultValue(
        "You are an expert %1 code completion AI."
        "CRITICAL: Please provide minimal the best possible code completion suggestions.\n");

    resetToDefaults.m_buttonText = Tr::tr("Reset to Defaults");

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

    topK.setEnabled(useTopK());
    topP.setEnabled(useTopP());
    presencePenalty.setEnabled(usePresencePenalty());
    frequencyPenalty.setEnabled(useFrequencyPenalty());
    readStringsAfterCursor.setEnabled(!readFullFile());
    readStringsBeforeCursor.setEnabled(!readFullFile());
    PromptTemplateManager::instance().setCurrentTemplate(fimPrompts.stringValue());
    LLMProvidersManager::instance().setCurrentProvider(llmProviders.stringValue());
    updateProviderSettings();

    setLoggingEnabled(enableLogging());

    setLayouter([this]() {
        using namespace Layouting;

        return Column{Group{title(Tr::tr("General Settings")),
                            Form{Column{enableQodeAssist,
                                        enableAutoComplete,
                                        enableLogging,
                                        Row{Stretch{1}, resetToDefaults}}}},
                      Group{title(Tr::tr("LLM Providers")),
                            Form{Column{llmProviders, Row{url, port, endPoint}, providerPaths}}},
                      Group{title(Tr::tr("LLM Model Settings")),
                            Form{Column{Row{selectModels, modelName}}}},
                      Group{title(Tr::tr("FIM Prompt Settings")),
                            Form{Column{fimPrompts,
                                        readFullFile,
                                        maxFileThreshold,
                                        ollamaLivetime,
                                        specificInstractions,
                                        temperature,
                                        maxTokens,
                                        readStringsBeforeCursor,
                                        readStringsAfterCursor,
                                        startSuggestionTimer,
                                        Row{useTopP, topP, Stretch{1}},
                                        Row{useTopK, topK, Stretch{1}},
                                        Row{usePresencePenalty, presencePenalty, Stretch{1}},
                                        Row{useFrequencyPenalty, frequencyPenalty, Stretch{1}}}}},
                      st};
    });

    setupConnections();
}

void QodeAssistSettings::setupConnections()
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
    connect(&useTopP, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        topP.setEnabled(useTopP.volatileValue());
    });
    connect(&useTopK, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        topK.setEnabled(useTopK.volatileValue());
    });
    connect(&usePresencePenalty, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        presencePenalty.setEnabled(usePresencePenalty.volatileValue());
    });
    connect(&useFrequencyPenalty, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        frequencyPenalty.setEnabled(useFrequencyPenalty.volatileValue());
    });
    connect(&readFullFile, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        readStringsAfterCursor.setEnabled(!readFullFile.volatileValue());
        readStringsBeforeCursor.setEnabled(!readFullFile.volatileValue());
    });
    connect(&resetToDefaults,
            &ButtonAspect::clicked,
            this,
            &QodeAssistSettings::resetSettingsToDefaults);
    connect(&enableLogging, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        setLoggingEnabled(enableLogging.volatileValue());
    });
}

void QodeAssistSettings::updateProviderSettings()
{
    auto *provider = LLMProvidersManager::instance().getCurrentProvider();

    if (provider) {
        logMessage(QString("currentProvider %1").arg(provider->name()));
        url.setValue(provider->url());
        port.setValue(provider->defaultPort());
        endPoint.setValue(provider->completionEndpoint());
        ollamaLivetime.setEnabled(provider->name() == "Ollama");
    }
}

QStringList QodeAssistSettings::getInstalledModels()
{
    auto *provider = LLMProvidersManager::instance().getCurrentProvider();
    if (provider) {
        auto env = getEnvironmentWithProviderPaths();
        return provider->getInstalledModels(env);
    }
    return {};
}

void QodeAssistSettings::showModelSelectionDialog()
{
    QStringList models = getInstalledModels();

    bool ok;
    QString selectedModel = QInputDialog::getItem(Core::ICore::dialogParent(),
                                                  Tr::tr("Select LLM Model"),
                                                  Tr::tr("Choose a model:"),
                                                  models,
                                                  0,
                                                  false,
                                                  &ok);

    if (ok && !selectedModel.isEmpty()) {
        modelName.setValue(selectedModel);
        writeSettings();
    }
}

Utils::Environment QodeAssistSettings::getEnvironmentWithProviderPaths() const
{
    Utils::Environment env = Utils::Environment::systemEnvironment();
    const QStringList additionalPaths = providerPaths.volatileValue();
    for (const QString &path : additionalPaths) {
        env.prependOrSetPath(path);
    }
    return env;
}

void QodeAssistSettings::resetSettingsToDefaults()
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
        resetAspect(port);
        resetAspect(endPoint);
        resetAspect(modelName);
        resetAspect(fimPrompts);
        resetAspect(temperature);
        resetAspect(maxTokens);
        resetAspect(readFullFile);
        resetAspect(maxFileThreshold);
        resetAspect(readStringsBeforeCursor);
        resetAspect(readStringsAfterCursor);
        resetAspect(useTopP);
        resetAspect(topP);
        resetAspect(useTopK);
        resetAspect(topK);
        resetAspect(usePresencePenalty);
        resetAspect(presencePenalty);
        resetAspect(useFrequencyPenalty);
        resetAspect(frequencyPenalty);
        resetAspect(startSuggestionTimer);
        resetAspect(enableLogging);
        resetAspect(ollamaLivetime);
        resetAspect(specificInstractions);

        updateProviderSettings();
        apply();

        QMessageBox::information(Core::ICore::dialogParent(),
                                 Tr::tr("Settings Reset"),
                                 Tr::tr("All settings have been reset to their default values."));
    }
}

class QodeAssistSettingsPage : public Core::IOptionsPage
{
public:
    QodeAssistSettingsPage()
    {
        setId(Constants::QODE_ASSIST_GENERAL_OPTIONS_ID);
        setDisplayName("Qode Assist");
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setDisplayCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY);
        setCategoryIconPath(":/resources/images/qoderassist-icon.png");
        setSettingsProvider([] { return &settings(); });
    }
};

const QodeAssistSettingsPage settingsPage;

} // namespace QodeAssist
