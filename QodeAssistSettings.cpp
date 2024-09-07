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

#include <QFileDialog>
#include <QInputDialog>
#include <QJsonParseError>
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

    temperature.setSettingsKey(Constants::TEMPERATURE);
    temperature.setLabelText(Tr::tr("Temperature:"));
    temperature.setDefaultValue(0.2);
    temperature.setRange(0.0, 10.0);


    ollamaLivetime.setSettingsKey(Constants::OLLAMA_LIVETIME);
    ollamaLivetime.setLabelText(
        Tr::tr("Time to suspend Ollama after completion request (in minutes), "
               "Only Ollama,  -1 to disable"));
    ollamaLivetime.setDefaultValue("5m");
    ollamaLivetime.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    maxTokens.setSettingsKey(Constants::MAX_TOKENS);
    maxTokens.setLabelText(Tr::tr("Max Tokens"));
    maxTokens.setRange(-1, 10000);
    maxTokens.setDefaultValue(150);

    useTopP.setSettingsKey(Constants::USE_TOP_P);
    useTopP.setDefaultValue(false);

    topP.setSettingsKey(Constants::TOP_P);
    topP.setLabelText(Tr::tr("top_p"));
    topP.setDefaultValue(0.9);
    topP.setRange(0.0, 1.0);

    useTopK.setSettingsKey(Constants::USE_TOP_K);
    useTopK.setDefaultValue(false);

    topK.setSettingsKey(Constants::TOP_K);
    topK.setLabelText(Tr::tr("top_k"));
    topK.setDefaultValue(50);
    topK.setRange(1, 1000);

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

    startSuggestionTimer.setSettingsKey(Constants::START_SUGGESTION_TIMER);
    startSuggestionTimer.setLabelText(Tr::tr("Start Suggestion Timer:"));
    startSuggestionTimer.setRange(10, 10000);
    startSuggestionTimer.setDefaultValue(500);

    resetToDefaults.m_buttonText = Tr::tr("Reset to Defaults");

    apiKey.setSettingsKey(Constants::API_KEY);
    apiKey.setLabelText(Tr::tr("API Key:"));
    apiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    apiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));

    customJsonTemplate.setSettingsKey(Constants::CUSTOM_JSON_TEMPLATE);
    customJsonTemplate.setLabelText("Custom JSON Template:");
    customJsonTemplate.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    customJsonTemplate.setDefaultValue(R"({
  "prompt": "{{QODE_INSTRUCTIONS}}<fim_prefix>{{QODE_PREFIX}}<fim_suffix>{{QODE_SUFFIX}}<fim_middle>",
  "options": {
    "temperature": 0.7,
    "top_p": 0.95,
    "top_k": 40,
    "num_predict": 100,
    "stop": [
      "<|endoftext|>",
      "<file_sep>",
      "<fim_prefix>",
      "<fim_suffix>",
      "<fim_middle>"
    ],
    "frequency_penalty": 0,
    "presence_penalty": 0
  },
  "stream": true
})");

    saveCustomTemplateButton.m_buttonText = (Tr::tr("Save Custom Template to JSON"));
    loadCustomTemplateButton.m_buttonText = (Tr::tr("Load Custom Template from JSON"));

    readSettings();

    topK.setEnabled(useTopK());
    topP.setEnabled(useTopP());
    presencePenalty.setEnabled(usePresencePenalty());
    frequencyPenalty.setEnabled(useFrequencyPenalty());

    customJsonTemplate.setVisible(PromptTemplateManager::instance().getCurrentTemplate()->name()
                                  == "Custom Template");

    setLayouter([this]() {
        using namespace Layouting;

        return Column{Group{title(Tr::tr("FIM Prompt Settings")),
                            Form{Column{Column{customJsonTemplate,
                                               Row{saveCustomTemplateButton,
                                                   loadCustomTemplateButton,
                                                   Stretch{1}}},
                                        // readFullFile,
                                        // maxFileThreshold,
                                        // readStringsBeforeCursor,
                                        // readStringsAfterCursor,
                                        ollamaLivetime,
                                        apiKey,
                                        // useFilePathInContext,
                                        // useSpecificInstructions,
                                        // specificInstractions,
                                        temperature,
                                        maxTokens,
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

    connect(&resetToDefaults,
            &ButtonAspect::clicked,
            this,
            &QodeAssistSettings::resetSettingsToDefaults);

    connect(&saveCustomTemplateButton,
            &ButtonAspect::clicked,
            this,
            &QodeAssistSettings::saveCustomTemplate);
    connect(&loadCustomTemplateButton,
            &ButtonAspect::clicked,
            this,
            &QodeAssistSettings::loadCustomTemplate);
}

QStringList QodeAssistSettings::getInstalledModels()
{
    auto *provider = LLMProvidersManager::instance().getCurrentProvider();
    if (provider) {
        Utils::Environment env = Utils::Environment::systemEnvironment();
        return provider->getInstalledModels(env);
    }
    return {};
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
        // resetAspect(enableQodeAssist);
        // resetAspect(enableAutoComplete);
        // resetAspect(llmProviders);
        // resetAspect(url);
        // resetAspect(endPoint);
        // resetAspect(modelName);
        // resetAspect(fimPrompts);
        resetAspect(temperature);
        resetAspect(maxTokens);
        // resetAspect(readFullFile);
        // resetAspect(maxFileThreshold);
        // resetAspect(readStringsBeforeCursor);
        // resetAspect(readStringsAfterCursor);
        resetAspect(useTopP);
        resetAspect(topP);
        resetAspect(useTopK);
        resetAspect(topK);
        resetAspect(usePresencePenalty);
        resetAspect(presencePenalty);
        resetAspect(useFrequencyPenalty);
        resetAspect(frequencyPenalty);
        resetAspect(startSuggestionTimer);
        // resetAspect(enableLogging);
        resetAspect(ollamaLivetime);
        // resetAspect(specificInstractions);
        // resetAspect(multiLineCompletion);
        // resetAspect(useFilePathInContext);
        // resetAspect(useSpecificInstructions);
        resetAspect(customJsonTemplate);

        // fimPrompts.setStringValue("StarCoder2");
        // llmProviders.setStringValue("Ollama");

        apply();

        QMessageBox::information(Core::ICore::dialogParent(),
                                 Tr::tr("Settings Reset"),
                                 Tr::tr("All settings have been reset to their default values."));
    }
}

void QodeAssistSettings::saveCustomTemplate()
{
    QString fileName = QFileDialog::getSaveFileName(nullptr,
                                                    Tr::tr("Save JSON Template"),
                                                    QString(),
                                                    Tr::tr("JSON Files (*.json)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << customJsonTemplate.value();
        file.close();
        QMessageBox::information(nullptr,
                                 Tr::tr("Save Successful"),
                                 Tr::tr("JSON template has been saved successfully."));
    } else {
        QMessageBox::critical(nullptr,
                              Tr::tr("Save Failed"),
                              Tr::tr("Failed to save JSON template."));
    }
}

void QodeAssistSettings::loadCustomTemplate()
{
    QString fileName = QFileDialog::getOpenFileName(nullptr,
                                                    Tr::tr("Load JSON Template"),
                                                    QString(),
                                                    Tr::tr("JSON Files (*.json)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString jsonContent = in.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonContent.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            customJsonTemplate.setValue(jsonContent);
            QMessageBox::information(nullptr,
                                     Tr::tr("Load Successful"),
                                     Tr::tr("JSON template has been loaded successfully."));
        } else {
            QMessageBox::critical(nullptr,
                                  Tr::tr("Invalid JSON"),
                                  Tr::tr("The selected file contains invalid JSON."));
        }
    } else {
        QMessageBox::critical(nullptr,
                              Tr::tr("Load Failed"),
                              Tr::tr("Failed to load JSON template."));
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
