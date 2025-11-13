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

#include "ToolsSettings.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include <QMessageBox>

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"

namespace QodeAssist::Settings {

ToolsSettings &toolsSettings()
{
    static ToolsSettings settings;
    return settings;
}

ToolsSettings::ToolsSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Tools"));

    useTools.setSettingsKey(Constants::CA_USE_TOOLS);
    useTools.setLabelText(Tr::tr("Enable tools"));
    useTools.setToolTip(Tr::tr(
            "Enable tool use capabilities for the assistant (OpenAI function calling, Claude tools "
            "and etc) if plugin and provider support"));
    useTools.setDefaultValue(true);

    allowFileSystemRead.setSettingsKey(Constants::CA_ALLOW_FILE_SYSTEM_READ);
    allowFileSystemRead.setLabelText(Tr::tr("Allow File System Read Access for tools"));
    allowFileSystemRead.setToolTip(
        Tr::tr("Allow tools to read files from disk (project files, open editors)"));
    allowFileSystemRead.setDefaultValue(true);

    allowFileSystemWrite.setSettingsKey(Constants::CA_ALLOW_FILE_SYSTEM_WRITE);
    allowFileSystemWrite.setLabelText(Tr::tr("Allow File System Write Access for tools"));
    allowFileSystemWrite.setToolTip(
        Tr::tr("Allow tools to write and modify files on disk (WARNING: Use with caution!)"));
    allowFileSystemWrite.setDefaultValue(false);

    allowAccessOutsideProject.setSettingsKey(Constants::CA_ALLOW_ACCESS_OUTSIDE_PROJECT);
    allowAccessOutsideProject.setLabelText(Tr::tr("Allow file access outside project"));
    allowAccessOutsideProject.setToolTip(
        Tr::tr("Allow tools to access (read/write) files outside the project scope (system "
               "headers, Qt files, external libraries)"));
    allowAccessOutsideProject.setDefaultValue(true);

    autoApplyFileEdits.setSettingsKey(Constants::CA_AUTO_APPLY_FILE_EDITS);
    autoApplyFileEdits.setLabelText(Tr::tr("Automatically apply file edits"));
    autoApplyFileEdits.setToolTip(
        Tr::tr("When enabled, file edits suggested by AI will be applied automatically. "
               "When disabled, you will need to manually approve each edit."));
    autoApplyFileEdits.setDefaultValue(false);

    enableEditFileTool.setSettingsKey(Constants::CA_ENABLE_EDIT_FILE_TOOL);
    enableEditFileTool.setLabelText(Tr::tr("Enable Edit File Tool (Experimental)"));
    enableEditFileTool.setToolTip(
        Tr::tr("Enable the experimental edit_file tool that allows AI to directly modify files. "
               "This feature is under testing and may have unexpected behavior."));
    enableEditFileTool.setDefaultValue(false);

    enableBuildProjectTool.setSettingsKey(Constants::CA_ENABLE_BUILD_PROJECT_TOOL);
    enableBuildProjectTool.setLabelText(Tr::tr("Enable Build Project Tool (Experimental)"));
    enableBuildProjectTool.setToolTip(
        Tr::tr("Enable the experimental build_project tool that allows AI to build the current "
               "project. This feature is under testing and may have unexpected behavior."));
    enableBuildProjectTool.setDefaultValue(false);

    debugToolsAndThinkingComponent.setSettingsKey(Constants::CA_DEBUG_TOOLS_AND_THINKING_COMPONENT);
    debugToolsAndThinkingComponent.setLabelText(Tr::tr("Always show Tools and Thinking Components in chat"));
    debugToolsAndThinkingComponent.setToolTip(
        Tr::tr("Disable disapearing tools and thinking component from chat"));
    debugToolsAndThinkingComponent.setDefaultValue(false);

    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    readSettings();

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        return Column{
            Row{Stretch{1}, resetToDefaults},
            Space{8},
            Group{
                title(Tr::tr("Tool Settings")),
                Column{
                    useTools,
                    Space{8},
                    allowFileSystemRead,
                    allowFileSystemWrite,
                    allowAccessOutsideProject,
                    debugToolsAndThinkingComponent
                }},
            Space{8},
            Group{
                title(Tr::tr("Experimental Features")),
                Column{enableEditFileTool, enableBuildProjectTool, autoApplyFileEdits}},
            Stretch{1}};
    });
}

void ToolsSettings::setupConnections()
{
    connect(
        &resetToDefaults,
        &ButtonAspect::clicked,
        this,
        &ToolsSettings::resetSettingsToDefaults);
}

void ToolsSettings::resetSettingsToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(useTools);
        resetAspect(allowFileSystemRead);
        resetAspect(allowFileSystemWrite);
        resetAspect(allowAccessOutsideProject);
        resetAspect(autoApplyFileEdits);
        resetAspect(enableEditFileTool);
        resetAspect(enableBuildProjectTool);
        resetAspect(debugToolsAndThinkingComponent);
        writeSettings();
    }
}

class ToolsSettingsPage : public Core::IOptionsPage
{
public:
    ToolsSettingsPage()
    {
        setId(Constants::QODE_ASSIST_TOOLS_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Tools"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &toolsSettings(); });
    }
};

const ToolsSettingsPage toolsSettingsPage;

} // namespace QodeAssist::Settings
