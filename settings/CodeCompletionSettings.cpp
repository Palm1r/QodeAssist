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
#include <logger/Logger.hpp>

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

    completionTriggerMode.setLabelText(Tr::tr("Completion trigger mode:"));
    completionTriggerMode.setSettingsKey(Constants::CC_COMPLETION_TRIGGER_MODE);
    completionTriggerMode.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    completionTriggerMode.addOption("Hint-based (Tab to trigger)");
    completionTriggerMode.addOption("Automatic");
    completionTriggerMode.setDefaultValue("Automatic");
    completionTriggerMode.setToolTip(
        Tr::tr("Hint-based: Shows a hint when typing, press Tab to request completion\n"
               "Automatic: Automatically requests completion after typing threshold"));

    startSuggestionTimer.setSettingsKey(Constants::СС_START_SUGGESTION_TIMER);
    startSuggestionTimer.setLabelText(Tr::tr("with delay(ms)"));
    startSuggestionTimer.setToolTip(
        Tr::tr("Delay before sending the completion request.\n"
               "(Only for Automatic trigger mode)"));
    startSuggestionTimer.setRange(10, 10000);
    startSuggestionTimer.setDefaultValue(350);

    autoCompletionCharThreshold.setSettingsKey(Constants::СС_AUTO_COMPLETION_CHAR_THRESHOLD);
    autoCompletionCharThreshold.setLabelText(Tr::tr("AI suggestion triggers after typing"));
    autoCompletionCharThreshold.setToolTip(
        Tr::tr("The number of characters that need to be typed within the typing interval "
               "before an AI suggestion request is sent automatically.\n"
               "(Only for Automatic trigger mode)"));
    autoCompletionCharThreshold.setRange(0, 10);
    autoCompletionCharThreshold.setDefaultValue(1);

    autoCompletionTypingInterval.setSettingsKey(Constants::СС_AUTO_COMPLETION_TYPING_INTERVAL);
    autoCompletionTypingInterval.setLabelText(Tr::tr("character(s) within(ms)"));
    autoCompletionTypingInterval.setToolTip(
        Tr::tr("The time window (in milliseconds) during which the character threshold "
               "must be met to trigger an AI suggestion request automatically.\n"
               "(Only for Automatic trigger mode)"));
    autoCompletionTypingInterval.setRange(500, 5000);
    autoCompletionTypingInterval.setDefaultValue(1200);

    hintCharThreshold.setSettingsKey(Constants::CC_HINT_CHAR_THRESHOLD);
    hintCharThreshold.setLabelText(Tr::tr("Hint shows after typing"));
    hintCharThreshold.setToolTip(
        Tr::tr("The number of characters that need to be typed before the hint widget appears "
               "(only for Hint-based trigger mode)."));
    hintCharThreshold.setRange(1, 10);
    hintCharThreshold.setDefaultValue(3);

    hintHideTimeout.setSettingsKey(Constants::CC_HINT_HIDE_TIMEOUT);
    hintHideTimeout.setLabelText(Tr::tr("Hint auto-hide timeout (ms)"));
    hintHideTimeout.setToolTip(
        Tr::tr("Time in milliseconds after which the hint widget will automatically hide "
               "(only for Hint-based trigger mode)."));
    hintHideTimeout.setRange(500, 10000);
    hintHideTimeout.setDefaultValue(4000);

    hintTriggerKey.setLabelText(Tr::tr("Trigger key:"));
    hintTriggerKey.setSettingsKey(Constants::CC_HINT_TRIGGER_KEY);
    hintTriggerKey.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    hintTriggerKey.addOption("Space");
    hintTriggerKey.addOption("Ctrl+Space");
    hintTriggerKey.addOption("Alt+Space");
    hintTriggerKey.addOption("Ctrl+Enter");
    hintTriggerKey.addOption("Tab");
    hintTriggerKey.addOption("Enter");
    hintTriggerKey.setDefaultValue("Tab");
    hintTriggerKey.setToolTip(
        Tr::tr("Key to press for requesting completion when hint is visible.\n"
               "Space is recommended as least conflicting with context menu.\n"
               "(Only for Hint-based trigger mode)"));

    ignoreWhitespaceInCharCount.setSettingsKey(Constants::CC_IGNORE_WHITESPACE_IN_CHAR_COUNT);
    ignoreWhitespaceInCharCount.setLabelText(
        Tr::tr("Ignore spaces and tabs in character count"));
    ignoreWhitespaceInCharCount.setDefaultValue(true);
    ignoreWhitespaceInCharCount.setToolTip(
        Tr::tr("When enabled, spaces and tabs are not counted towards the character threshold "
               "for triggering completions. This helps trigger completions based on actual code "
               "characters only."));

    // General Parameters Settings
    temperature.setSettingsKey(Constants::CC_TEMPERATURE);
    temperature.setLabelText(Tr::tr("Temperature:"));
    temperature.setDefaultValue(0.2);
    temperature.setRange(0.0, 2.0);
    temperature.setSingleStep(0.1);

    maxTokens.setSettingsKey(Constants::CC_MAX_TOKENS);
    maxTokens.setLabelText(Tr::tr("Max Tokens:"));
    maxTokens.setRange(-1, 900000);
    maxTokens.setDefaultValue(500);

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

    abortAssistOnRequest.setSettingsKey(Constants::CC_ABORT_ASSIST_ON_REQUEST);
    abortAssistOnRequest.setLabelText(Tr::tr("Abort existing assist on new completion request"));
    abortAssistOnRequest.setToolTip(
        Tr::tr("When enabled, cancels any active Qt Creator code assist popup "
               "before requesting LLM completion.\n"
               "(Only for Automatic trigger mode)"));
    abortAssistOnRequest.setDefaultValue(true);

    useOpenFilesContext.setSettingsKey(Constants::CC_USE_OPEN_FILES_CONTEXT);
    useOpenFilesContext.setLabelText(Tr::tr("Include context from open files"));
    useOpenFilesContext.setDefaultValue(false);

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

    // OpenAI Responses API Settings
    openAIResponsesReasoningEffort.setSettingsKey(Constants::CC_OPENAI_RESPONSES_REASONING_EFFORT);
    openAIResponsesReasoningEffort.setLabelText(Tr::tr("Reasoning effort:"));
    openAIResponsesReasoningEffort.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    openAIResponsesReasoningEffort.addOption("None");
    openAIResponsesReasoningEffort.addOption("Minimal");
    openAIResponsesReasoningEffort.addOption("Low");
    openAIResponsesReasoningEffort.addOption("Medium");
    openAIResponsesReasoningEffort.addOption("High");
    openAIResponsesReasoningEffort.setDefaultValue("Medium");
    openAIResponsesReasoningEffort.setToolTip(
        Tr::tr("Constrains effort on reasoning for OpenAI gpt-5 and o-series models:\n\n"
               "None: No reasoning (gpt-5.1 only)\n"
               "Minimal: Minimal reasoning effort (o-series only)\n"
               "Low: Low reasoning effort\n"
               "Medium: Balanced reasoning (default for most models)\n"
               "High: Maximum reasoning effort (gpt-5-pro only supports this)\n\n"
               "Note: Reducing effort = faster responses + fewer tokens"));

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

        auto openAIResponsesGrid = Grid{};
        openAIResponsesGrid.addRow({openAIResponsesReasoningEffort});

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

        auto generalSettings = Column{
            autoCompletion,
            multiLineCompletion,
            Row{modelOutputHandler, Stretch{1}},
            Row{completionTriggerMode, Stretch{1}},
            showProgressWidget,
            useOpenFilesContext,
            abortAssistOnRequest,
            ignoreWhitespaceInCharCount};

        auto autoTriggerSettings = Column{
            Row{autoCompletionCharThreshold,
                autoCompletionTypingInterval,
                startSuggestionTimer,
                Stretch{1}}};

        auto hintTriggerSettings = Column{
            Row{hintCharThreshold, hintHideTimeout, Stretch{1}},
            Row{hintTriggerKey, Stretch{1}}};

        return Column{Row{Stretch{1}, resetToDefaults},
                      Space{8},
                      Group{title(TrConstants::AUTO_COMPLETION_SETTINGS),
                            Column{Group{title(Tr::tr("General Settings")), generalSettings},
                                   Space{8},
                                   Group{title(Tr::tr("Automatic Trigger Mode")), autoTriggerSettings},
                                   Space{8},
                                   Group{title(Tr::tr("Hint-based Trigger Mode")), hintTriggerSettings}}},
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
                      Group{title(Tr::tr("OpenAI Responses API")), Column{Row{openAIResponsesGrid, Stretch{1}}}},
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
        resetAspect(openAIResponsesReasoningEffort);
        resetAspect(useUserMessageTemplateForCC);
        resetAspect(userMessageTemplateForCC);
        resetAspect(systemPromptForNonFimModels);
        resetAspect(customLanguages);
        resetAspect(showProgressWidget);
        resetAspect(useOpenFilesContext);
        resetAspect(modelOutputHandler);
        resetAspect(completionTriggerMode);
        resetAspect(hintCharThreshold);
        resetAspect(hintHideTimeout);
        resetAspect(hintTriggerKey);
        resetAspect(ignoreWhitespaceInCharCount);
        resetAspect(abortAssistOnRequest);
        writeSettings();
    }
}

QString CodeCompletionSettings::processMessageToFIM(const QString &prefix, const QString &suffix) const
{
    QString result = userMessageTemplateForCC();

    // Always replace basic tags
    result.replace("${prefix}", prefix);
    result.replace("${suffix}", suffix);

    // Check if extra tags are needed
    bool needsExtraTags = result.contains("${cursor_line}") ||
                          result.contains("${prefix-1}") ||
                          result.contains("${suffix-1}");
    if (needsExtraTags) {
        // Extract line part from prefix (last line before cursor)
        int lastNewlineInPrefix = prefix.lastIndexOf('\n');
        QString head = (lastNewlineInPrefix == -1) ? prefix : prefix.mid(lastNewlineInPrefix + 1);

        // Extract line part from suffix (first line after cursor)
        int firstNewlineInSuffix = suffix.indexOf('\n');
        QString tail = (firstNewlineInSuffix == -1) ? suffix : suffix.left(firstNewlineInSuffix);

        // Build cursor_line (cursor line without newlines)
        QString cursor_line = head + tail;

        // Build prefix-1 (prefix without cursor line head)
        QString prefix_1 = (lastNewlineInPrefix != -1) ? prefix.left(lastNewlineInPrefix + 1) : "";

        // Build suffix-1 (suffix without cursor line tail)
        QString suffix_1 = (firstNewlineInSuffix != -1) ? suffix.mid(firstNewlineInSuffix) : "";

        // Replace new tags
        LOG_MESSAGE("Applying extended placeholders: cursor_line, prefix-1, suffix-1");
        LOG_MESSAGE(QString("cursor_line=\n'%1'").arg(cursor_line));
        result.replace("${cursor_line}", cursor_line);
        result.replace("${prefix-1}", prefix_1);
        result.replace("${suffix-1}", suffix_1);
    }

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
