// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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

    completionTriggerMode.setLabelText(Tr::tr("Completion trigger mode:"));
    completionTriggerMode.setSettingsKey(Constants::CC_COMPLETION_TRIGGER_MODE);
    completionTriggerMode.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    completionTriggerMode.addOption("Hint-based (Tab to trigger)");
    completionTriggerMode.addOption("Automatic");
    completionTriggerMode.setDefaultValue("Automatic");
    completionTriggerMode.setToolTip(
        Tr::tr("Hint-based: Shows a hint when typing, press Tab to request completion\n"
               "Automatic: Automatically requests completion after typing threshold"));

    completionMode.setLabelText(Tr::tr("Completion mode:"));
    completionMode.setSettingsKey(Constants::CC_COMPLETION_MODE);
    completionMode.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    completionMode.addOption("Automatic");
    completionMode.addOption("Manual");
    completionMode.setDefaultValue("Automatic");
    completionMode.setToolTip(
        Tr::tr("Automatic: requests completion while typing (with smart context gates).\n"
               "Manual: no auto-triggering; invoke via the 'Request QodeAssist Suggestion' "
               "shortcut (default Ctrl+Alt+Q, reconfigurable in Preferences > Keyboard)."));

    smartContextTrigger.setSettingsKey(Constants::CC_SMART_CONTEXT_TRIGGER);
    smartContextTrigger.setLabelText(Tr::tr("Smart context-aware triggering"));
    smartContextTrigger.setDefaultValue(true);
    smartContextTrigger.setToolTip(
        Tr::tr("When enabled, auto-completion is suppressed in places where Qt Creator's built-in "
               "completion is usually stronger (middle of an identifier, right after '.', '->', "
               "'::') and is triggered more eagerly after structural characters like '(', ',', "
               "'{', '=' and on fresh indented lines."));

    respectQtcPopup.setSettingsKey(Constants::CC_RESPECT_QTC_POPUP);
    respectQtcPopup.setLabelText(Tr::tr("Don't dismiss Qt Creator's completion popup"));
    respectQtcPopup.setDefaultValue(true);
    respectQtcPopup.setToolTip(
        Tr::tr("When enabled, an AI completion arriving while Qt Creator's own completion popup "
               "is already visible will not force it closed. The LLM suggestion still appears "
               "inline."));

    cancelOnInput.setSettingsKey(Constants::CC_CANCEL_ON_INPUT);
    cancelOnInput.setLabelText(Tr::tr("Cancel in-flight request on new input"));
    cancelOnInput.setDefaultValue(false);
    cancelOnInput.setToolTip(
        Tr::tr("When enabled, every new keystroke cancels any completion request already in "
               "flight and restarts the debounce timer. Useful for slow local models where an "
               "outdated answer is rarely worth waiting for.\n"
               "When disabled (default), the in-flight request is kept; when the answer arrives, "
               "the plugin compares it with characters typed in the meantime and either trims "
               "the matching prefix or drops the answer."));

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

    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    readSettings();

    migrateCompletionMode();

    readFileParts.setValue(!readFullFile.value());

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        auto contextGrid = Grid{};
        contextGrid.addRow({Row{readFullFile}});
        contextGrid.addRow({Row{readFileParts, readStringsBeforeCursor, readStringsAfterCursor}});

        auto contextItem = Column{
            Row{contextGrid, Stretch{1}},
            customLanguages,
            Row{useProjectChangesCache, maxChangesCacheSize, Stretch{1}}};

        auto generalSettings = Column{
            autoCompletion,
            multiLineCompletion,
            Row{modelOutputHandler, Stretch{1}},
            Row{completionMode, Stretch{1}},
            showProgressWidget,
            useOpenFilesContext,
            respectQtcPopup,
            cancelOnInput,
            abortAssistOnRequest,
            ignoreWhitespaceInCharCount};

        auto autoTriggerSettings = Column{
            smartContextTrigger,
            Row{autoCompletionCharThreshold,
                autoCompletionTypingInterval,
                startSuggestionTimer,
                Stretch{1}}};

        return Column{Row{Stretch{1}, resetToDefaults},
                      Space{8},
                      Group{title(TrConstants::AUTO_COMPLETION_SETTINGS),
                            Column{Group{title(Tr::tr("General Settings")), generalSettings},
                                   Space{8},
                                   Group{title(Tr::tr("Automatic Trigger Mode")), autoTriggerSettings}}},
                      Space{8},
                      Group{title(Tr::tr("Context Settings")), contextItem},
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
        resetAspect(readFullFile);
        resetAspect(readFileParts);
        resetAspect(readStringsBeforeCursor);
        resetAspect(readStringsAfterCursor);
        resetAspect(useProjectChangesCache);
        resetAspect(maxChangesCacheSize);
        resetAspect(customLanguages);
        resetAspect(showProgressWidget);
        resetAspect(useOpenFilesContext);
        resetAspect(modelOutputHandler);
        resetAspect(completionTriggerMode);
        resetAspect(completionMode);
        resetAspect(smartContextTrigger);
        resetAspect(respectQtcPopup);
        resetAspect(cancelOnInput);
        resetAspect(hintCharThreshold);
        resetAspect(hintHideTimeout);
        resetAspect(hintTriggerKey);
        resetAspect(ignoreWhitespaceInCharCount);
        resetAspect(abortAssistOnRequest);
        writeSettings();
    }
}

void CodeCompletionSettings::migrateCompletionMode()
{
    auto *qtcSettings = Core::ICore::settings();
    if (!qtcSettings || qtcSettings->contains(Constants::CC_COMPLETION_MODE))
        return;

    const QString oldMode = completionTriggerMode.stringValue();
    if (oldMode.startsWith("Hint"))
        completionMode.setStringValue("Manual");
    else
        completionMode.setStringValue("Automatic");

    writeSettings();
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
