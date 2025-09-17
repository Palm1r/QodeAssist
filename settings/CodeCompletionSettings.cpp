/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include <QMessageBox>

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

    modelOutputHandler.setLabelText(Tr::tr("Text output proccessing mode:"));
    modelOutputHandler.setSettingsKey(Constants::CC_MODEL_OUTPUT_HANDLER);
    modelOutputHandler.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    modelOutputHandler.addOption("Auto");
    modelOutputHandler.addOption("Force processing");
    modelOutputHandler.addOption("Raw text");
    modelOutputHandler.setDefaultValue("Auto");
    modelOutputHandler.setToolTip(
        Tr::tr("Auto: Automatically detects codeblock and applies processing when found, other "
               "text as comments\n"
               "Force Processing: Always processes text with codeblock formatting and other text "
               "as comments\n"
               "Raw Text: Shows unprocessed text without any formatting"));

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
    maxTokens.setRange(-1, 900000);
    maxTokens.setDefaultValue(100);

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
        "You are an expert C++, Qt, and QML code completion assistant. Your task is to provide "
        "precise and contextually appropriate code completions.\n\n");

    useUserMessageTemplateForCC.setSettingsKey(Constants::CC_USE_USER_TEMPLATE);
    useUserMessageTemplateForCC.setDefaultValue(true);
    useUserMessageTemplateForCC.setLabelText(
        Tr::tr("Use special system prompt and user message for non FIM models"));

    systemPromptForNonFimModels.setSettingsKey(Constants::CC_SYSTEM_PROMPT_FOR_NON_FIM);
    systemPromptForNonFimModels.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    systemPromptForNonFimModels.setLabelText(Tr::tr("System prompt for non FIM models:"));
    systemPromptForNonFimModels.setDefaultValue(
        "You are an expert C++, Qt, and QML code completion assistant. Your task is to provide "
        "precise and contextually appropriate code completions.\n\n"
        "Core Requirements:\n"
        "1. Continue code exactly from the cursor position, ensuring it properly connects with any "
        "existing code after the cursor\n"
        "2. Never repeat existing code before or after the cursor\n"
        "Specific Guidelines:\n"
        "- For function calls: Complete parameters with appropriate types and names\n"
        "- For class members: Respect access modifiers and class conventions\n"
        "- Respect existing indentation and formatting\n"
        "- Consider scope and visibility of referenced symbols\n"
        "- Ensure seamless integration with code both before and after the cursor\n\n"
        "Context Format:\n"
        "<code_context>\n"
        "{{code before cursor}}<cursor>{{code after cursor}}\n"
        "</code_context>\n\n"
        "Response Format:\n"
        "- No explanations or comments\n"
        "- Only include new characters needed to create valid code\n"
        "- Should be codeblock with language\n");

    userMessageTemplateForCC.setSettingsKey(Constants::CC_USER_TEMPLATE);
    userMessageTemplateForCC.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    userMessageTemplateForCC.setLabelText(Tr::tr("User message for non FIM models:"));
    userMessageTemplateForCC.setDefaultValue(
        "Here is the code context with insertion points:\n"
        "<code_context>\n${prefix}<cursor>${suffix}\n</code_context>\n\n");

    customLanguages.setSettingsKey(Constants::CC_CUSTOM_LANGUAGES);
    customLanguages.setLabelText(
        Tr::tr("Additional Programming Languages for handling: Example: rust,//,rust rs,rs"));
    customLanguages.setToolTip(Tr::tr("Specify additional programming languages in format: "
                                      "name,comment_style,model_names,extensions\n"
                                      "Example: rust,//,rust rs,rs\n"
                                      "Fields: language name, comment prefix, names from LLM "
                                      "(space-separated), file extensions (space-separated)"));
    customLanguages.setDefaultValue({{"cmake,#,cmake,CMakeLists.txt"}, {"qmake,#,qmake,pro pri"}});

    showProgressWidget.setSettingsKey(Constants::CC_SHOW_PROGRESS_WIDGET);
    showProgressWidget.setLabelText(Tr::tr("Show progress indicator during code completion"));
    showProgressWidget.setDefaultValue(true);

    useOpenFilesContext.setSettingsKey(Constants::CC_USE_OPEN_FILES_CONTEXT);
    useOpenFilesContext.setLabelText(Tr::tr("Include context from open files"));
    useOpenFilesContext.setDefaultValue(false);

    useProjectChangesCache.setSettingsKey(Constants::CC_USE_PROJECT_CHANGES_CACHE);
    useProjectChangesCache.setDefaultValue(true);
    useProjectChangesCache.setLabelText(Tr::tr("Max Changes Cache Size:"));

    maxChangesCacheSize.setSettingsKey(Constants::CC_MAX_CHANGES_CACHE_SIZE);
    maxChangesCacheSize.setRange(2, 1000);
    maxChangesCacheSize.setDefaultValue(10);

    // Quick refactor command settings
    useOpenFilesInQuickRefactor.setSettingsKey(Constants::CC_USE_OPEN_FILES_IN_QUICK_REFACTOR);
    useOpenFilesInQuickRefactor.setLabelText(
        Tr::tr("Include context from open files in quick refactor"));
    useOpenFilesInQuickRefactor.setDefaultValue(false);
    quickRefactorSystemPrompt.setSettingsKey(Constants::CC_QUICK_REFACTOR_SYSTEM_PROMPT);
    quickRefactorSystemPrompt.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    quickRefactorSystemPrompt.setDefaultValue(
        "You are an expert C++, Qt, and QML code completion assistant. Your task is to provide"
        "precise and contextually appropriate code completions to insert depending on user "
        "instructions.\n\n");

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

        auto contextItem = Column{
            Row{contextGrid, Stretch{1}},
            Row{useSystemPrompt, Stretch{1}},
            Group{title(Tr::tr("Prompts for FIM models")), Column{systemPrompt}},
            Group{
                title(Tr::tr("Prompts for Non FIM models")),
                Column{
                    Row{useUserMessageTemplateForCC, Stretch{1}},
                    systemPromptForNonFimModels,
                    userMessageTemplateForCC,
                    customLanguages,
                }},
            Row{useProjectChangesCache, maxChangesCacheSize, Stretch{1}}};

        return Column{Row{Stretch{1}, resetToDefaults},
                      Space{8},
                      Group{title(TrConstants::AUTO_COMPLETION_SETTINGS),
                            Column{autoCompletion,
                                   Space{8},
                                   multiLineCompletion,
                                   Row{modelOutputHandler, Stretch{1}},
                                   Row{autoCompletionCharThreshold,
                                       autoCompletionTypingInterval,
                                       startSuggestionTimer,
                                       Stretch{1}},
                                   showProgressWidget,
                                   useOpenFilesContext}},
                      Space{8},
                      Group{title(Tr::tr("General Parameters")),
                            Column{
                                Row{genGrid, Stretch{1}},
                            }},
                      Space{8},
                      Group{title(Tr::tr("Advanced Parameters")),
                            Column{Row{advancedGrid, Stretch{1}}}},
                      Space{8},
                      Group{title(Tr::tr("Context Settings")), contextItem},
                      Space{8},
                      Group{title(Tr::tr("Quick Refactor Settings")),
                            Column{useOpenFilesInQuickRefactor, quickRefactorSystemPrompt}},
                      Space{8},
                      Group{title(Tr::tr("Ollama Settings")), Column{Row{ollamaGrid, Stretch{1}}}},
                      Stretch{1}};
    });
}

void CodeCompletionSettings::setupConnections()
{
    connect(
        &resetToDefaults,
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
        resetAspect(useProjectChangesCache);
        resetAspect(maxChangesCacheSize);
        resetAspect(ollamaLivetime);
        resetAspect(contextWindow);
        resetAspect(useUserMessageTemplateForCC);
        resetAspect(userMessageTemplateForCC);
        resetAspect(systemPromptForNonFimModels);
        resetAspect(customLanguages);
        resetAspect(showProgressWidget);
        resetAspect(useOpenFilesContext);
        resetAspect(useOpenFilesInQuickRefactor);
        resetAspect(quickRefactorSystemPrompt);
        resetAspect(modelOutputHandler);
    }
}

QString CodeCompletionSettings::processMessageToFIM(const QString &prefix, const QString &suffix) const
{
    QString result = userMessageTemplateForCC();
    result.replace("${prefix}", prefix);
    result.replace("${suffix}", suffix);
    return result;
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
