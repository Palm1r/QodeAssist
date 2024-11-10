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

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"

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

    fimTemperature.setSettingsKey(Constants::CC_TEMPERATURE);
    fimTemperature.setLabelText(Tr::tr("Temperature:"));
    fimTemperature.setDefaultValue(0.2);
    fimTemperature.setRange(0.0, 10.0);
    fimTemperature.setSingleStep(0.1);

    chatTemperature.setSettingsKey(Constants::CHAT_TEMPERATURE);
    chatTemperature.setLabelText(Tr::tr("Temperature:"));
    chatTemperature.setDefaultValue(0.5);
    chatTemperature.setRange(0.0, 10.0);
    chatTemperature.setSingleStep(0.1);

    fimOllamaLivetime.setSettingsKey(Constants::CC_OLLAMA_LIVETIME);
    fimOllamaLivetime.setLabelText(
        Tr::tr("Time to suspend Ollama after completion request (in minutes), "
               "Only Ollama,  -1 to disable"));
    fimOllamaLivetime.setDefaultValue("5m");
    fimOllamaLivetime.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    chatOllamaLivetime.setSettingsKey(Constants::CHAT_OLLAMA_LIVETIME);
    chatOllamaLivetime.setLabelText(
        Tr::tr("Time to suspend Ollama after completion request (in minutes), "
               "Only Ollama,  -1 to disable"));
    chatOllamaLivetime.setDefaultValue("5m");
    chatOllamaLivetime.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    fimMaxTokens.setSettingsKey(Constants::CC_MAX_TOKENS);
    fimMaxTokens.setLabelText(Tr::tr("Max Tokens"));
    fimMaxTokens.setRange(-1, 10000);
    fimMaxTokens.setDefaultValue(50);

    chatMaxTokens.setSettingsKey(Constants::CHAT_MAX_TOKENS);
    chatMaxTokens.setLabelText(Tr::tr("Max Tokens"));
    chatMaxTokens.setRange(-1, 10000);
    chatMaxTokens.setDefaultValue(2000);

    fimUseTopP.setSettingsKey(Constants::CC_USE_TOP_P);
    fimUseTopP.setDefaultValue(false);

    fimTopP.setSettingsKey(Constants::CC_TOP_P);
    fimTopP.setLabelText(Tr::tr("use top_p"));
    fimTopP.setDefaultValue(0.9);
    fimTopP.setRange(0.0, 1.0);
    fimTopP.setSingleStep(0.1);

    chatUseTopP.setSettingsKey(Constants::CHAT_USE_TOP_P);
    chatUseTopP.setDefaultValue(false);

    chatTopP.setSettingsKey(Constants::CHAT_TOP_P);
    chatTopP.setLabelText(Tr::tr("use top_p"));
    chatTopP.setDefaultValue(0.9);
    chatTopP.setRange(0.0, 1.0);
    chatTopP.setSingleStep(0.1);

    fimUseTopK.setSettingsKey(Constants::CC_USE_TOP_K);
    fimUseTopK.setDefaultValue(false);

    fimTopK.setSettingsKey(Constants::CC_TOP_K);
    fimTopK.setLabelText(Tr::tr("use top_k"));
    fimTopK.setDefaultValue(50);
    fimTopK.setRange(1, 1000);

    chatUseTopK.setSettingsKey(Constants::CHAT_USE_TOP_K);
    chatUseTopK.setDefaultValue(false);

    chatTopK.setSettingsKey(Constants::CHAT_TOP_K);
    chatTopK.setLabelText(Tr::tr("use top_k"));
    chatTopK.setDefaultValue(50);
    chatTopK.setRange(1, 1000);

    fimUsePresencePenalty.setSettingsKey(Constants::CC_USE_PRESENCE_PENALTY);
    fimUsePresencePenalty.setDefaultValue(false);

    fimPresencePenalty.setSettingsKey(Constants::CC_PRESENCE_PENALTY);
    fimPresencePenalty.setLabelText(Tr::tr("use presence_penalty"));
    fimPresencePenalty.setDefaultValue(0.0);
    fimPresencePenalty.setRange(-2.0, 2.0);
    fimPresencePenalty.setSingleStep(0.1);

    chatUsePresencePenalty.setSettingsKey(Constants::CHAT_USE_PRESENCE_PENALTY);
    chatUsePresencePenalty.setDefaultValue(false);

    chatPresencePenalty.setSettingsKey(Constants::CHAT_PRESENCE_PENALTY);
    chatPresencePenalty.setLabelText(Tr::tr("use presence_penalty"));
    chatPresencePenalty.setDefaultValue(0.0);
    chatPresencePenalty.setRange(-2.0, 2.0);
    chatPresencePenalty.setSingleStep(0.1);

    fimUseFrequencyPenalty.setSettingsKey(Constants::CC_USE_FREQUENCY_PENALTY);
    fimUseFrequencyPenalty.setDefaultValue(false);

    fimFrequencyPenalty.setSettingsKey(Constants::CC_FREQUENCY_PENALTY);
    fimFrequencyPenalty.setLabelText(Tr::tr("use frequency_penalty"));
    fimFrequencyPenalty.setDefaultValue(0.0);
    fimFrequencyPenalty.setRange(-2.0, 2.0);
    fimFrequencyPenalty.setSingleStep(0.1);

    chatUseFrequencyPenalty.setSettingsKey(Constants::CHAT_USE_FREQUENCY_PENALTY);
    chatUseFrequencyPenalty.setDefaultValue(false);

    chatFrequencyPenalty.setSettingsKey(Constants::CHAT_FREQUENCY_PENALTY);
    chatFrequencyPenalty.setLabelText(Tr::tr("use frequency_penalty"));
    chatFrequencyPenalty.setDefaultValue(0.0);
    chatFrequencyPenalty.setRange(-2.0, 2.0);
    chatFrequencyPenalty.setSingleStep(0.1);

    fimApiKey.setSettingsKey(Constants::CC_API_KEY);
    fimApiKey.setLabelText(Tr::tr("API Key:"));
    fimApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    fimApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));

    chatApiKey.setSettingsKey(Constants::CHAT_API_KEY);
    chatApiKey.setLabelText(Tr::tr("API Key:"));
    chatApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    chatApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));

    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    readSettings();

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;
        return Column{Row{Stretch{1}, resetToDefaults},
                      Group{title(Tr::tr("Prompt settings for FIM")),
                            Column{Row{fimTemperature, Stretch{1}},
                                   Row{fimMaxTokens, Stretch{1}},
                                   Row{fimUseTopP, fimTopP, Stretch{1}},
                                   Row{fimUseTopK, fimTopK, Stretch{1}},
                                   Row{fimUsePresencePenalty, fimPresencePenalty, Stretch{1}},
                                   Row{fimUseFrequencyPenalty, fimFrequencyPenalty, Stretch{1}},
                                   Row{fimOllamaLivetime, Stretch{1}},
                                   fimApiKey}},
                      Space{16},
                      Group{title(Tr::tr("Prompt settings for Chat")),
                            Column{Row{chatTemperature, Stretch{1}},
                                   Row{chatMaxTokens, Stretch{1}},
                                   Row{chatUseTopP, chatTopP, Stretch{1}},
                                   Row{chatUseTopK, chatTopK, Stretch{1}},
                                   Row{fimUsePresencePenalty, fimPresencePenalty, Stretch{1}},
                                   Row{fimUseFrequencyPenalty, fimFrequencyPenalty, Stretch{1}},
                                   Row{chatOllamaLivetime, Stretch{1}},
                                   chatApiKey}},
                      Stretch{1}};
    });
}

PresetPromptsSettings::PromptSettings PresetPromptsSettings::getSettings(int type) const
{
    PromptSettings settings;
    if (type == 0) {
        settings.temperature = fimTemperature();
        settings.maxTokens = fimMaxTokens();
        settings.useTopP = fimUseTopP();
        settings.topP = fimTopP();
        settings.useTopK = fimUseTopK();
        settings.topK = fimTopK();
        settings.usePresencePenalty = fimUsePresencePenalty();
        settings.presencePenalty = fimPresencePenalty();
        settings.useFrequencyPenalty = fimUseFrequencyPenalty();
        settings.frequencyPenalty = fimFrequencyPenalty();
        settings.ollamaLivetime = fimOllamaLivetime();
        settings.apiKey = fimApiKey();
    } else if (type = 1) {
        settings.temperature = chatTemperature();
        settings.maxTokens = chatMaxTokens();
        settings.useTopP = chatUseTopP();
        settings.topP = chatTopP();
        settings.useTopK = chatUseTopK();
        settings.topK = chatTopK();
        settings.usePresencePenalty = chatUsePresencePenalty();
        settings.presencePenalty = chatPresencePenalty();
        settings.useFrequencyPenalty = chatUseFrequencyPenalty();
        settings.frequencyPenalty = chatFrequencyPenalty();
        settings.ollamaLivetime = chatOllamaLivetime();
        settings.apiKey = chatApiKey();
    }
    return settings;
}

void PresetPromptsSettings::setupConnections()
{
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
        resetAspect(fimTemperature);
        resetAspect(fimMaxTokens);
        resetAspect(fimOllamaLivetime);
        resetAspect(fimUseTopP);
        resetAspect(fimTopP);
        resetAspect(fimUseTopK);
        resetAspect(fimTopK);
        resetAspect(fimUsePresencePenalty);
        resetAspect(fimPresencePenalty);
        resetAspect(fimUseFrequencyPenalty);
        resetAspect(fimFrequencyPenalty);
        resetAspect(chatTemperature);
        resetAspect(chatMaxTokens);
        resetAspect(chatUseTopP);
        resetAspect(chatTopP);
        resetAspect(chatUseTopK);
        resetAspect(chatTopK);
        resetAspect(chatUsePresencePenalty);
        resetAspect(chatPresencePenalty);
        resetAspect(chatUseFrequencyPenalty);
        resetAspect(chatFrequencyPenalty);
        resetAspect(chatOllamaLivetime);
    }
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
