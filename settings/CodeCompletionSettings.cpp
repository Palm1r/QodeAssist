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

#include "CodeCompletionSettings.hpp"

#include <QMessageBox>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"

namespace QodeAssist::Settings {

CodeCompletionSettings &codeCompletionSettings()
{
    static CodeCompletionSettings settings;
    return settings;
}

CodeCompletionSettings::CodeCompletionSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Code Completion"));

    // Auto Completion Settings
    autoCompletion.setSettingsKey(Constants::CC_AUTO_COMPLETION);
    autoCompletion.setLabelText(Tr::tr("Enable Auto Complete"));
    autoCompletion.setDefaultValue(true);

    multiLineCompletion.setSettingsKey(Constants::CC_MULTILINE_COMPLETION);
    multiLineCompletion.setDefaultValue(true);
    multiLineCompletion.setLabelText(Tr::tr("Enable Multiline Completion"));

    stream.setSettingsKey(Constants::CC_STREAM);
    stream.setDefaultValue(true);
    stream.setLabelText(Tr::tr("Enable stream option"));

    smartProcessInstuctText.setSettingsKey(Constants::CC_SMART_PROCESS_INSTRUCT_TEXT);
    smartProcessInstuctText.setDefaultValue(true);
    smartProcessInstuctText.setLabelText(Tr::tr("Enable smart process text from instruct model"));

    startSuggestionTimer.setSettingsKey(Constants::СС_START_SUGGESTION_TIMER);
    startSuggestionTimer.setLabelText(Tr::tr("with delay(ms)"));
    startSuggestionTimer.setRange(10, 10000);
    startSuggestionTimer.setDefaultValue(350);

    autoCompletionCharThreshold.setSettingsKey(Constants::СС_AUTO_COMPLETION_CHAR_THRESHOLD);
    autoCompletionCharThreshold.setLabelText(Tr::tr("AI suggestion triggers after typing"));
    autoCompletionCharThreshold.setToolTip(
        Tr::tr("The number of characters that need to be typed within the typing interval "
               "before an AI suggestion request is sent."));
    autoCompletionCharThreshold.setRange(0, 10);
    autoCompletionCharThreshold.setDefaultValue(1);

    autoCompletionTypingInterval.setSettingsKey(Constants::СС_AUTO_COMPLETION_TYPING_INTERVAL);
    autoCompletionTypingInterval.setLabelText(Tr::tr("character(s) within(ms)"));
    autoCompletionTypingInterval.setToolTip(
        Tr::tr("The time window (in milliseconds) during which the character threshold "
               "must be met to trigger an AI suggestion request."));
    autoCompletionTypingInterval.setRange(500, 5000);
    autoCompletionTypingInterval.setDefaultValue(1200);

    // General Parameters Settings
    temperature.setSettingsKey(Constants::CC_TEMPERATURE);
    temperature.setLabelText(Tr::tr("Temperature:"));
    temperature.setDefaultValue(0.2);
    temperature.setRange(0.0, 2.0);
    temperature.setSingleStep(0.1);

    maxTokens.setSettingsKey(Constants::CC_MAX_TOKENS);
    maxTokens.setLabelText(Tr::tr("Max Tokens:"));
    maxTokens.setRange(-1, 10000);
    maxTokens.setDefaultValue(50);

    // Advanced Parameters
    useTopP.setSettingsKey(Constants::CC_USE_TOP_P);
    useTopP.setDefaultValue(false);
    useTopP.setLabelText(Tr::tr("Top P:"));

    topP.setSettingsKey(Constants::CC_TOP_P);
    topP.setDefaultValue(0.9);
    topP.setRange(0.0, 1.0);
    topP.setSingleStep(0.1);

    useTopK.setSettingsKey(Constants::CC_USE_TOP_K);
    useTopK.setDefaultValue(false);
    useTopK.setLabelText(Tr::tr("Top K:"));

    topK.setSettingsKey(Constants::CC_TOP_K);
    topK.setDefaultValue(50);
    topK.setRange(1, 1000);

    usePresencePenalty.setSettingsKey(Constants::CC_USE_PRESENCE_PENALTY);
    usePresencePenalty.setDefaultValue(false);
    usePresencePenalty.setLabelText(Tr::tr("Presence Penalty:"));

    presencePenalty.setSettingsKey(Constants::CC_PRESENCE_PENALTY);
    presencePenalty.setDefaultValue(0.0);
    presencePenalty.setRange(-2.0, 2.0);
    presencePenalty.setSingleStep(0.1);

    useFrequencyPenalty.setSettingsKey(Constants::CC_USE_FREQUENCY_PENALTY);
    useFrequencyPenalty.setDefaultValue(false);
    useFrequencyPenalty.setLabelText(Tr::tr("Frequency Penalty:"));

    frequencyPenalty.setSettingsKey(Constants::CC_FREQUENCY_PENALTY);
    frequencyPenalty.setDefaultValue(0.0);
    frequencyPenalty.setRange(-2.0, 2.0);
    frequencyPenalty.setSingleStep(0.1);

    // Context Settings
    readFullFile.setSettingsKey(Constants::CC_READ_FULL_FILE);
    readFullFile.setLabelText(Tr::tr("Read Full File"));
    readFullFile.setDefaultValue(false);

    readFileParts.setLabelText(Tr::tr("Read Strings Before Cursor:"));
    readFileParts.setDefaultValue(true);

    readStringsBeforeCursor.setSettingsKey(Constants::CC_READ_STRINGS_BEFORE_CURSOR);
    readStringsBeforeCursor.setRange(0, 10000);
    readStringsBeforeCursor.setDefaultValue(50);

    readStringsAfterCursor.setSettingsKey(Constants::CC_READ_STRINGS_AFTER_CURSOR);
    readStringsAfterCursor.setLabelText(Tr::tr("Read Strings After Cursor:"));
    readStringsAfterCursor.setRange(0, 10000);
    readStringsAfterCursor.setDefaultValue(30);

    useSystemPrompt.setSettingsKey(Constants::CC_USE_SYSTEM_PROMPT);
    useSystemPrompt.setDefaultValue(true);
    useSystemPrompt.setLabelText(Tr::tr("Use System Prompt"));

    systemPrompt.setSettingsKey(Constants::CC_SYSTEM_PROMPT);
    systemPrompt.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    systemPrompt.setDefaultValue(
        "You are an expert C++, Qt, and QML. You insert the code into the areas where the user "
        "specifies. In answer should be ONLY code suggestions in code block, without comments or "
        "description. Don't repeat existing code. Complete ONLY one logic expression.");

    useFilePathInContext.setSettingsKey(Constants::CC_USE_FILE_PATH_IN_CONTEXT);
    useFilePathInContext.setDefaultValue(true);
    useFilePathInContext.setLabelText(Tr::tr("Use File Path in Context"));

    useProjectChangesCache.setSettingsKey(Constants::CC_USE_PROJECT_CHANGES_CACHE);
    useProjectChangesCache.setDefaultValue(true);
    useProjectChangesCache.setLabelText(Tr::tr("Max Changes Cache Size:"));

    maxChangesCacheSize.setSettingsKey(Constants::CC_MAX_CHANGES_CACHE_SIZE);
    maxChangesCacheSize.setRange(2, 1000);
    maxChangesCacheSize.setDefaultValue(10);

    // Ollama Settings
    ollamaLivetime.setSettingsKey(Constants::CC_OLLAMA_LIVETIME);
    ollamaLivetime.setToolTip(
        Tr::tr("Time to suspend Ollama after completion request (in minutes), "
               "Only Ollama,  -1 to disable"));
    ollamaLivetime.setLabelText("Livetime:");
    ollamaLivetime.setDefaultValue("5m");
    ollamaLivetime.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    contextWindow.setSettingsKey(Constants::CC_OLLAMA_CONTEXT_WINDOW);
    contextWindow.setLabelText(Tr::tr("Context Window:"));
    contextWindow.setRange(-1, 10000);
    contextWindow.setDefaultValue(2048);

    // API Configuration Settings
    apiKey.setSettingsKey(Constants::CC_API_KEY);
    apiKey.setLabelText(Tr::tr("API Key:"));
    apiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    apiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));

    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    readSettings();

    readFileParts.setValue(!readFullFile.value());

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        auto genGrid = Grid{};
        genGrid.addRow({Row{temperature}});
        genGrid.addRow({Row{maxTokens}});

        auto advancedGrid = Grid{};
        advancedGrid.addRow({useTopP, topP});
        advancedGrid.addRow({useTopK, topK});
        advancedGrid.addRow({usePresencePenalty, presencePenalty});
        advancedGrid.addRow({useFrequencyPenalty, frequencyPenalty});

        auto ollamaGrid = Grid{};
        ollamaGrid.addRow({ollamaLivetime});
        ollamaGrid.addRow({contextWindow});

        auto contextGrid = Grid{};
        contextGrid.addRow({Row{readFullFile}});
        contextGrid.addRow({Row{readFileParts, readStringsBeforeCursor, readStringsAfterCursor}});

        auto contextItem = Column{Row{contextGrid, Stretch{1}},
                                  Row{useSystemPrompt, Stretch{1}},
                                  systemPrompt,
                                  Row{useFilePathInContext, Stretch{1}},
                                  Row{useProjectChangesCache, maxChangesCacheSize, Stretch{1}}};

        return Column{
            Row{Stretch{1}, resetToDefaults},
            Space{8},
            Group{
                title(TrConstants::AUTO_COMPLETION_SETTINGS),
                Column{
                    autoCompletion,
                    Space{8},
                    multiLineCompletion,
                    stream,
                    smartProcessInstuctText,
                    Row{autoCompletionCharThreshold,
                        autoCompletionTypingInterval,
                        startSuggestionTimer,
                        Stretch{1}}}},
            Space{8},
            Group{
                title(Tr::tr("General Parameters")),
                Column{
                    Row{genGrid, Stretch{1}},
                }},
            Space{8},
            Group{title(Tr::tr("Advanced Parameters")), Column{Row{advancedGrid, Stretch{1}}}},
            Space{8},
            Group{title(Tr::tr("Context Settings")), contextItem},
            Space{8},
            Group{title(Tr::tr("Ollama Settings")), Column{Row{ollamaGrid, Stretch{1}}}},
            Space{8},
            Group{title(Tr::tr("API Configuration")), Column{apiKey}},
            Stretch{1}};
    });
}

void CodeCompletionSettings::setupConnections()
{
    connect(&resetToDefaults,
            &ButtonAspect::clicked,
            this,
            &CodeCompletionSettings::resetSettingsToDefaults);

    connect(&readFullFile, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        if (readFullFile.volatileValue()) {
            readFileParts.setValue(false);
            writeSettings();
        }
    });

    connect(&readFileParts, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        if (readFileParts.volatileValue()) {
            readFullFile.setValue(false);
            writeSettings();
        }
    });
}

void CodeCompletionSettings::resetSettingsToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(autoCompletion);
        resetAspect(multiLineCompletion);
        resetAspect(stream);
        resetAspect(temperature);
        resetAspect(maxTokens);
        resetAspect(useTopP);
        resetAspect(topP);
        resetAspect(useTopK);
        resetAspect(topK);
        resetAspect(usePresencePenalty);
        resetAspect(presencePenalty);
        resetAspect(useFrequencyPenalty);
        resetAspect(frequencyPenalty);
        resetAspect(readFullFile);
        resetAspect(readFileParts);
        resetAspect(readStringsBeforeCursor);
        resetAspect(readStringsAfterCursor);
        resetAspect(useSystemPrompt);
        resetAspect(systemPrompt);
        resetAspect(useFilePathInContext);
        resetAspect(useProjectChangesCache);
        resetAspect(maxChangesCacheSize);
        resetAspect(ollamaLivetime);
        resetAspect(contextWindow);
    }
}

class CodeCompletionSettingsPage : public Core::IOptionsPage
{
public:
    CodeCompletionSettingsPage()
    {
        setId(Constants::QODE_ASSIST_CODE_COMPLETION_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Code Completion"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &codeCompletionSettings(); });
    }
};

const CodeCompletionSettingsPage codeCompletionSettingsPage;

} // namespace QodeAssist::Settings
