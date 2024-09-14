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

#pragma once

#include "ContextSettings.hpp"

#include <QMessageBox>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>

#include "QodeAssistConstants.hpp"
#include "QodeAssisttr.h"

namespace QodeAssist::Settings {
ContextSettings &contextSettings()
{
    static ContextSettings settings;
    return settings;
}

ContextSettings::ContextSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Context"));

    readFullFile.setSettingsKey(Constants::READ_FULL_FILE);
    readFullFile.setLabelText(Tr::tr("Read Full File"));
    readFullFile.setDefaultValue(false);

    readStringsBeforeCursor.setSettingsKey(Constants::READ_STRINGS_BEFORE_CURSOR);
    readStringsBeforeCursor.setLabelText(Tr::tr("Read Strings Before Cursor"));
    readStringsBeforeCursor.setRange(0, 10000);
    readStringsBeforeCursor.setDefaultValue(50);

    readStringsAfterCursor.setSettingsKey(Constants::READ_STRINGS_AFTER_CURSOR);
    readStringsAfterCursor.setLabelText(Tr::tr("Read Strings After Cursor"));
    readStringsAfterCursor.setRange(0, 10000);
    readStringsAfterCursor.setDefaultValue(30);

    useFilePathInContext.setSettingsKey(Constants::USE_FILE_PATH_IN_CONTEXT);
    useFilePathInContext.setDefaultValue(false);
    useFilePathInContext.setLabelText(Tr::tr("Use File Path in Context"));

    useSpecificInstructions.setSettingsKey(Constants::USE_SPECIFIC_INSTRUCTIONS);
    useSpecificInstructions.setDefaultValue(true);
    useSpecificInstructions.setLabelText(Tr::tr("Use Specific Instructions"));

    specificInstractions.setSettingsKey(Constants::SPECIFIC_INSTRUCTIONS);
    specificInstractions.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    specificInstractions.setLabelText(
        Tr::tr("Instructions: Please keep %1 for languge name, warning, it shouldn't too big"));
    specificInstractions.setDefaultValue(
        "You are an expert %1 code completion AI."
        "CRITICAL: Please provide minimal the best possible code completion suggestions.\n");

    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    useProjectChangesCache.setSettingsKey(Constants::USE_PROJECT_CHANGES_CACHE);
    useProjectChangesCache.setDefaultValue(true);
    useProjectChangesCache.setLabelText(Tr::tr("Use Project Changes Cache"));

    maxChangesCacheSize.setSettingsKey(Constants::MAX_CHANGES_CACHE_SIZE);
    maxChangesCacheSize.setLabelText(Tr::tr("Max Changes Cache Size"));
    maxChangesCacheSize.setRange(2, 1000);
    maxChangesCacheSize.setDefaultValue(20);

    readSettings();

    readStringsAfterCursor.setEnabled(!readFullFile());
    readStringsBeforeCursor.setEnabled(!readFullFile());
    specificInstractions.setEnabled(useSpecificInstructions());

    setupConnection();

    setLayouter([this]() {
        using namespace Layouting;
        return Column{Row{readFullFile, Stretch{1}, resetToDefaults},
                      Row{readStringsBeforeCursor, Stretch{1}},
                      Row{readStringsAfterCursor, Stretch{1}},
                      useFilePathInContext,
                      useSpecificInstructions,
                      specificInstractions,
                      useProjectChangesCache,
                      Row{maxChangesCacheSize, Stretch{1}},
                      Stretch{1}};
    });
}

void ContextSettings::setupConnection()
{
    connect(&readFullFile, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        readStringsAfterCursor.setEnabled(!readFullFile.volatileValue());
        readStringsBeforeCursor.setEnabled(!readFullFile.volatileValue());
    });
    connect(&useSpecificInstructions, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        specificInstractions.setEnabled(useSpecificInstructions.volatileValue());
    });
    connect(&resetToDefaults, &ButtonAspect::clicked, this, &ContextSettings::resetPageToDefaults);
}

void ContextSettings::resetPageToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(readFullFile);
        resetAspect(readStringsBeforeCursor);
        resetAspect(readStringsAfterCursor);
        resetAspect(useFilePathInContext);
        resetAspect(useSpecificInstructions);
        resetAspect(specificInstractions);
    }
}

class ContextSettingsPage : public Core::IOptionsPage
{
public:
    ContextSettingsPage()
    {
        setId(Constants::QODE_ASSIST_CONTEXT_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Context"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setDisplayCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY);
        setCategoryIconPath(":/resources/images/qoderassist-icon.png");
        setSettingsProvider([] { return &contextSettings(); });
    }
};

const ContextSettingsPage contextSettingsPage;

} // namespace QodeAssist::Settings
