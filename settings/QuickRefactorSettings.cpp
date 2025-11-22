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

#include "QuickRefactorSettings.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include <QMessageBox>

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"

namespace QodeAssist::Settings {

QuickRefactorSettings &quickRefactorSettings()
{
    static QuickRefactorSettings settings;
    return settings;
}

QuickRefactorSettings::QuickRefactorSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Quick Refactor"));

    // General Parameters Settings
    temperature.setSettingsKey(Constants::QR_TEMPERATURE);
    temperature.setLabelText(Tr::tr("Temperature:"));
    temperature.setDefaultValue(0.5);
    temperature.setRange(0.0, 2.0);
    temperature.setSingleStep(0.1);

    maxTokens.setSettingsKey(Constants::QR_MAX_TOKENS);
    maxTokens.setLabelText(Tr::tr("Max Tokens:"));
    maxTokens.setRange(-1, 200000);
    maxTokens.setDefaultValue(2000);

    // Advanced Parameters
    useTopP.setSettingsKey(Constants::QR_USE_TOP_P);
    useTopP.setDefaultValue(false);
    useTopP.setLabelText(Tr::tr("Top P:"));

    topP.setSettingsKey(Constants::QR_TOP_P);
    topP.setDefaultValue(0.9);
    topP.setRange(0.0, 1.0);
    topP.setSingleStep(0.1);

    useTopK.setSettingsKey(Constants::QR_USE_TOP_K);
    useTopK.setDefaultValue(false);
    useTopK.setLabelText(Tr::tr("Top K:"));

    topK.setSettingsKey(Constants::QR_TOP_K);
    topK.setDefaultValue(50);
    topK.setRange(1, 1000);

    usePresencePenalty.setSettingsKey(Constants::QR_USE_PRESENCE_PENALTY);
    usePresencePenalty.setDefaultValue(false);
    usePresencePenalty.setLabelText(Tr::tr("Presence Penalty:"));

    presencePenalty.setSettingsKey(Constants::QR_PRESENCE_PENALTY);
    presencePenalty.setDefaultValue(0.0);
    presencePenalty.setRange(-2.0, 2.0);
    presencePenalty.setSingleStep(0.1);

    useFrequencyPenalty.setSettingsKey(Constants::QR_USE_FREQUENCY_PENALTY);
    useFrequencyPenalty.setDefaultValue(false);
    useFrequencyPenalty.setLabelText(Tr::tr("Frequency Penalty:"));

    frequencyPenalty.setSettingsKey(Constants::QR_FREQUENCY_PENALTY);
    frequencyPenalty.setDefaultValue(0.0);
    frequencyPenalty.setRange(-2.0, 2.0);
    frequencyPenalty.setSingleStep(0.1);

    // Ollama Settings
    ollamaLivetime.setSettingsKey(Constants::QR_OLLAMA_LIVETIME);
    ollamaLivetime.setToolTip(
        Tr::tr("Time to suspend Ollama after completion request (in minutes), "
               "Only Ollama,  -1 to disable"));
    ollamaLivetime.setLabelText("Livetime:");
    ollamaLivetime.setDefaultValue("5m");
    ollamaLivetime.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    contextWindow.setSettingsKey(Constants::QR_OLLAMA_CONTEXT_WINDOW);
    contextWindow.setLabelText(Tr::tr("Context Window:"));
    contextWindow.setRange(-1, 10000);
    contextWindow.setDefaultValue(2048);

    useTools.setSettingsKey(Constants::QR_USE_TOOLS);
    useTools.setLabelText(Tr::tr("Enable Tools"));
    useTools.setToolTip(
        Tr::tr("Enable AI tools/functions for quick refactoring (allows reading project files, "
               "searching code, etc.)"));
    useTools.setDefaultValue(false);

    useThinking.setSettingsKey(Constants::QR_USE_THINKING);
    useThinking.setLabelText(Tr::tr("Enable Thinking Mode"));
    useThinking.setToolTip(
        Tr::tr("Enable extended thinking mode for complex refactoring tasks (supported by "
               "compatible models like Claude and Google AI)"));
    useThinking.setDefaultValue(false);

    thinkingBudgetTokens.setSettingsKey(Constants::QR_THINKING_BUDGET_TOKENS);
    thinkingBudgetTokens.setLabelText(Tr::tr("Thinking Budget Tokens:"));
    thinkingBudgetTokens.setToolTip(
        Tr::tr("Number of tokens allocated for thinking process. Use -1 for dynamic thinking "
               "(model decides), 0 to disable, or positive value for custom budget"));
    thinkingBudgetTokens.setRange(-1, 100000);
    thinkingBudgetTokens.setDefaultValue(10000);

    thinkingMaxTokens.setSettingsKey(Constants::QR_THINKING_MAX_TOKENS);
    thinkingMaxTokens.setLabelText(Tr::tr("Thinking Max Output Tokens:"));
    thinkingMaxTokens.setToolTip(
        Tr::tr("Maximum output tokens when thinking mode is enabled (includes thinking + response)"));
    thinkingMaxTokens.setRange(1000, 200000);
    thinkingMaxTokens.setDefaultValue(16000);

    // Context Settings
    readFullFile.setSettingsKey(Constants::QR_READ_FULL_FILE);
    readFullFile.setLabelText(Tr::tr("Read Full File"));
    readFullFile.setDefaultValue(false);

    readFileParts.setLabelText(Tr::tr("Read Strings Before Cursor:"));
    readFileParts.setDefaultValue(true);

    readStringsBeforeCursor.setSettingsKey(Constants::QR_READ_STRINGS_BEFORE_CURSOR);
    readStringsBeforeCursor.setLabelText(Tr::tr("Lines Before Cursor/Selection:"));
    readStringsBeforeCursor.setToolTip(
        Tr::tr("Number of lines to include before cursor or selection for context"));
    readStringsBeforeCursor.setRange(0, 10000);
    readStringsBeforeCursor.setDefaultValue(50);

    readStringsAfterCursor.setSettingsKey(Constants::QR_READ_STRINGS_AFTER_CURSOR);
    readStringsAfterCursor.setLabelText(Tr::tr("Lines After Cursor/Selection:"));
    readStringsAfterCursor.setToolTip(
        Tr::tr("Number of lines to include after cursor or selection for context"));
    readStringsAfterCursor.setRange(0, 10000);
    readStringsAfterCursor.setDefaultValue(30);

    displayMode.setSettingsKey(Constants::QR_DISPLAY_MODE);
    displayMode.setLabelText(Tr::tr("Display Mode:"));
    displayMode.setToolTip(
        Tr::tr("Choose how to display refactoring suggestions:\n"
               "- Inline Widget: Shows refactor in a widget overlay with Apply/Decline buttons (default)\n"
               "- Qt Creator Suggestion: Uses Qt Creator's built-in suggestion system"));
    displayMode.addOption(Tr::tr("Inline Widget"));
    displayMode.addOption(Tr::tr("Qt Creator Suggestion"));
    displayMode.setDefaultValue(0);

    widgetOrientation.setSettingsKey(Constants::QR_WIDGET_ORIENTATION);
    widgetOrientation.setLabelText(Tr::tr("Widget Orientation:"));
    widgetOrientation.setToolTip(
        Tr::tr("Choose default orientation for refactor widget:\n"
               "- Horizontal: Original and refactored code side by side (default)\n"
               "- Vertical: Original and refactored code stacked vertically"));
    widgetOrientation.addOption(Tr::tr("Horizontal"));
    widgetOrientation.addOption(Tr::tr("Vertical"));
    widgetOrientation.setDefaultValue(0);

    widgetMinWidth.setSettingsKey(Constants::QR_WIDGET_MIN_WIDTH);
    widgetMinWidth.setLabelText(Tr::tr("Widget Minimum Width:"));
    widgetMinWidth.setToolTip(Tr::tr("Minimum width for the refactor widget (in pixels)"));
    widgetMinWidth.setRange(400, 2000);
    widgetMinWidth.setDefaultValue(600);

    widgetMaxWidth.setSettingsKey(Constants::QR_WIDGET_MAX_WIDTH);
    widgetMaxWidth.setLabelText(Tr::tr("Widget Maximum Width:"));
    widgetMaxWidth.setToolTip(Tr::tr("Maximum width for the refactor widget (in pixels)"));
    widgetMaxWidth.setRange(600, 3000);
    widgetMaxWidth.setDefaultValue(1400);

    widgetMinHeight.setSettingsKey(Constants::QR_WIDGET_MIN_HEIGHT);
    widgetMinHeight.setLabelText(Tr::tr("Widget Minimum Height:"));
    widgetMinHeight.setToolTip(Tr::tr("Minimum height for the refactor widget (in pixels)"));
    widgetMinHeight.setRange(80, 800);
    widgetMinHeight.setDefaultValue(120);

    widgetMaxHeight.setSettingsKey(Constants::QR_WIDGET_MAX_HEIGHT);
    widgetMaxHeight.setLabelText(Tr::tr("Widget Maximum Height:"));
    widgetMaxHeight.setToolTip(Tr::tr("Maximum height for the refactor widget (in pixels)"));
    widgetMaxHeight.setRange(200, 1200);
    widgetMaxHeight.setDefaultValue(500);

    systemPrompt.setSettingsKey(Constants::QR_SYSTEM_PROMPT);
    systemPrompt.setLabelText(Tr::tr("System Prompt:"));
    systemPrompt.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    systemPrompt.setDefaultValue(
        "You are an expert C++, Qt, and QML code completion assistant. Your task is to provide "
        "precise and contextually appropriate code completions to insert depending on user "
        "instructions.\n\n");

    resetToDefaults.m_buttonText = TrConstants::RESET_TO_DEFAULTS;

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

        auto toolsGrid = Grid{};
        toolsGrid.addRow({useTools});
        toolsGrid.addRow({useThinking});
        toolsGrid.addRow({thinkingBudgetTokens});
        toolsGrid.addRow({thinkingMaxTokens});

        auto contextGrid = Grid{};
        contextGrid.addRow({Row{readFullFile}});
        contextGrid.addRow({Row{readFileParts, readStringsBeforeCursor, readStringsAfterCursor}});

        auto displayGrid = Grid{};
        displayGrid.addRow({Row{displayMode}});
        displayGrid.addRow({Row{widgetOrientation}});
        displayGrid.addRow({Row{widgetMinWidth, widgetMaxWidth}});
        displayGrid.addRow({Row{widgetMinHeight, widgetMaxHeight}});

        return Column{
            Row{Stretch{1}, resetToDefaults},
            Space{8},
            Group{
                title(Tr::tr("General Parameters")),
                Row{genGrid, Stretch{1}},
            },
            Space{8},
            Group{title(Tr::tr("Advanced Parameters")), Column{Row{advancedGrid, Stretch{1}}}},
            Space{8},
            Group{title(Tr::tr("Tools Settings")), Column{Row{toolsGrid, Stretch{1}}}},
            Space{8},
            Group{title(Tr::tr("Context Settings")), Column{Row{contextGrid, Stretch{1}}}},
            Space{8},
            Group{title(Tr::tr("Display Settings")), Column{Row{displayGrid, Stretch{1}}}},
            Space{8},
            Group{title(Tr::tr("Prompt Settings")), Column{Row{systemPrompt}}},
            Space{8},
            Group{title(Tr::tr("Ollama Settings")), Column{Row{ollamaGrid, Stretch{1}}}},
            Stretch{1}};
    });
}

void QuickRefactorSettings::setupConnections()
{
    connect(
        &resetToDefaults,
        &ButtonAspect::clicked,
        this,
        &QuickRefactorSettings::resetSettingsToDefaults);

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

    // Enable/disable widgetOrientation based on displayMode
    // 0 = Inline Widget, 1 = Qt Creator Suggestion
    auto updateWidgetOrientationEnabled = [this]() {
        bool isInlineWidget = (displayMode.volatileValue() == 0);
        widgetOrientation.setEnabled(isInlineWidget);
    };
    
    connect(&displayMode, &Utils::SelectionAspect::volatileValueChanged, 
            this, updateWidgetOrientationEnabled);
    
    updateWidgetOrientationEnabled();

    auto validateWidgetSizes = [this]() {
        if (widgetMinWidth.volatileValue() > widgetMaxWidth.volatileValue()) {
            widgetMaxWidth.setValue(widgetMinWidth.volatileValue());
        }
        if (widgetMinHeight.volatileValue() > widgetMaxHeight.volatileValue()) {
            widgetMaxHeight.setValue(widgetMinHeight.volatileValue());
        }
    };

    connect(&widgetMinWidth, &Utils::IntegerAspect::volatileValueChanged, this, validateWidgetSizes);
    connect(&widgetMaxWidth, &Utils::IntegerAspect::volatileValueChanged, this, validateWidgetSizes);
    connect(&widgetMinHeight, &Utils::IntegerAspect::volatileValueChanged, this, validateWidgetSizes);
    connect(&widgetMaxHeight, &Utils::IntegerAspect::volatileValueChanged, this, validateWidgetSizes);
}

void QuickRefactorSettings::resetSettingsToDefaults()
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
        resetAspect(useTopP);
        resetAspect(topP);
        resetAspect(useTopK);
        resetAspect(topK);
        resetAspect(usePresencePenalty);
        resetAspect(presencePenalty);
        resetAspect(useFrequencyPenalty);
        resetAspect(frequencyPenalty);
        resetAspect(ollamaLivetime);
        resetAspect(contextWindow);
        resetAspect(useTools);
        resetAspect(useThinking);
        resetAspect(thinkingBudgetTokens);
        resetAspect(thinkingMaxTokens);
        resetAspect(readFullFile);
        resetAspect(readFileParts);
        resetAspect(readStringsBeforeCursor);
        resetAspect(readStringsAfterCursor);
        resetAspect(displayMode);
        resetAspect(widgetOrientation);
        resetAspect(widgetMinWidth);
        resetAspect(widgetMaxWidth);
        resetAspect(widgetMinHeight);
        resetAspect(widgetMaxHeight);
        resetAspect(systemPrompt);
        writeSettings();
    }
}

class QuickRefactorSettingsPage : public Core::IOptionsPage
{
public:
    QuickRefactorSettingsPage()
    {
        setId(Constants::QODE_ASSIST_QUICK_REFACTOR_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Quick Refactor"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &quickRefactorSettings(); });
    }
};

const QuickRefactorSettingsPage quickRefactorSettingsPage;

} // namespace QodeAssist::Settings

