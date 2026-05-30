// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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
        Tr::tr(
            "Choose how to display refactoring suggestions:\n"
            "- Inline Widget: Shows refactor in a widget overlay with Apply/Decline buttons "
            "(default)\n"
            "- Qt Creator Suggestion: Uses Qt Creator's built-in suggestion system"));
    displayMode.addOption(Tr::tr("Inline Widget"));
    displayMode.addOption(Tr::tr("Qt Creator Suggestion"));
    displayMode.setDefaultValue(0);

    widgetOrientation.setSettingsKey(Constants::QR_WIDGET_ORIENTATION);
    widgetOrientation.setLabelText(Tr::tr("Widget Orientation:"));
    widgetOrientation.setToolTip(
        Tr::tr(
            "Choose default orientation for refactor widget:\n"
            "- Horizontal: Original and refactored code side by side (default)\n"
            "- Vertical: Original and refactored code stacked vertically"));
    widgetOrientation.addOption(Tr::tr("Horizontal"));
    widgetOrientation.addOption(Tr::tr("Vertical"));
    widgetOrientation.setDefaultValue(0);

    widgetMinWidth.setSettingsKey(Constants::QR_WIDGET_MIN_WIDTH);
    widgetMinWidth.setLabelText(Tr::tr("Widget Minimum Width:"));
    widgetMinWidth.setToolTip(Tr::tr("Minimum width for the refactor widget (in pixels)"));
    widgetMinWidth.setRange(200, 999999);
    widgetMinWidth.setDefaultValue(400);

    widgetMaxWidth.setSettingsKey(Constants::QR_WIDGET_MAX_WIDTH);
    widgetMaxWidth.setLabelText(Tr::tr("Widget Maximum Width:"));
    widgetMaxWidth.setToolTip(Tr::tr("Maximum width for the refactor widget (in pixels)"));
    widgetMaxWidth.setRange(400, 999999);
    widgetMaxWidth.setDefaultValue(9999);

    widgetMinHeight.setSettingsKey(Constants::QR_WIDGET_MIN_HEIGHT);
    widgetMinHeight.setLabelText(Tr::tr("Widget Minimum Height:"));
    widgetMinHeight.setToolTip(Tr::tr("Minimum height for the refactor widget (in pixels)"));
    widgetMinHeight.setRange(80, 999999);
    widgetMinHeight.setDefaultValue(100);

    widgetMaxHeight.setSettingsKey(Constants::QR_WIDGET_MAX_HEIGHT);
    widgetMaxHeight.setLabelText(Tr::tr("Widget Maximum Height:"));
    widgetMaxHeight.setToolTip(Tr::tr("Maximum height for the refactor widget (in pixels)"));
    widgetMaxHeight.setRange(200, 999999);
    widgetMaxHeight.setDefaultValue(9999);

    useOpenFilesInQuickRefactor.setSettingsKey(Constants::QR_USE_OPEN_FILES_IN_QUICK_REFACTOR);
    useOpenFilesInQuickRefactor.setLabelText(
        Tr::tr("Include context from open files in quick refactor"));
    useOpenFilesInQuickRefactor.setDefaultValue(false);

    resetToDefaults.m_buttonText = TrConstants::RESET_TO_DEFAULTS;

    readSettings();

    readFileParts.setValue(!readFullFile.value());

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        auto contextGrid = Grid{};
        contextGrid.addRow({Row{readFullFile}});
        contextGrid.addRow({
            Row{readFileParts, readStringsBeforeCursor, readStringsAfterCursor},
        });
        contextGrid.addRow({Row{useOpenFilesInQuickRefactor}});

        auto displayGrid = Grid{};
        displayGrid.addRow({Row{displayMode}});
        displayGrid.addRow({Row{widgetOrientation}});
        displayGrid.addRow({Row{widgetMinWidth, widgetMaxWidth}});
        displayGrid.addRow({Row{widgetMinHeight, widgetMaxHeight}});

        return Column{
            Row{Stretch{1}, resetToDefaults},
            Space{8},
            Group{title(Tr::tr("Context Settings")), Column{Row{contextGrid, Stretch{1}}}},
            Space{8},
            Group{title(Tr::tr("Display Settings")), Column{Row{displayGrid, Stretch{1}}}},
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

    connect(
        &displayMode,
        &Utils::SelectionAspect::volatileValueChanged,
        this,
        updateWidgetOrientationEnabled);

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
        resetAspect(useOpenFilesInQuickRefactor);
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
