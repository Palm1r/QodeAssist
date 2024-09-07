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

#include "PresetPromptsSettings.hpp"

#include <QMessageBox>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>

#include "QodeAssistConstants.hpp"
#include "QodeAssisttr.h"

namespace QodeAssist::Settings {

PresetPromptsSettings &presetPromptsSettings()
{
    static PresetPromptsSettings settings;
    return settings;
}

PresetPromptsSettings::PresetPromptsSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Preset Prompts Params"));

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

    apiKey.setSettingsKey(Constants::API_KEY);
    apiKey.setLabelText(Tr::tr("API Key:"));
    apiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    apiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));

    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    readSettings();

    topK.setEnabled(useTopK());
    topP.setEnabled(useTopP());
    presencePenalty.setEnabled(usePresencePenalty());
    frequencyPenalty.setEnabled(useFrequencyPenalty());

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;
        return Column{Row{temperature, Stretch{1}, resetToDefaults},
                      maxTokens,
                      Row{useTopP, topP, Stretch{1}},
                      Row{useTopK, topK, Stretch{1}},
                      Row{usePresencePenalty, presencePenalty, Stretch{1}},
                      Row{useFrequencyPenalty, frequencyPenalty, Stretch{1}},
                      apiKey,
                      Stretch{1}};
    });
}

void PresetPromptsSettings::setupConnections()
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
            &PresetPromptsSettings::resetSettingsToDefaults);
}

void PresetPromptsSettings::resetSettingsToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(temperature);
        resetAspect(maxTokens);
        resetAspect(ollamaLivetime);
        resetAspect(useTopP);
        resetAspect(topP);
        resetAspect(useTopK);
        resetAspect(topK);
        resetAspect(usePresencePenalty);
        resetAspect(presencePenalty);
        resetAspect(useFrequencyPenalty);
        resetAspect(frequencyPenalty);
    }

    QMessageBox::information(Core::ICore::dialogParent(),
                             Tr::tr("Settings Reset"),
                             Tr::tr("All settings have been reset to their default values."));
}

class PresetPromptsSettingsPage : public Core::IOptionsPage
{
public:
    PresetPromptsSettingsPage()
    {
        setId(Constants::QODE_ASSIST_PRESET_PROMPTS_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Preset Prompts Params"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &presetPromptsSettings(); });
    }
};

const PresetPromptsSettingsPage presetPromptsSettingsPage;

} // namespace QodeAssist::Settings
