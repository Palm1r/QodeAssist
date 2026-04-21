// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

    allowAccessOutsideProject.setSettingsKey(Constants::CA_ALLOW_ACCESS_OUTSIDE_PROJECT);
    allowAccessOutsideProject.setLabelText(Tr::tr("Allow file access outside project"));
    allowAccessOutsideProject.setToolTip(
        Tr::tr("Allow tools to read, write, and create files outside the project scope "
               "(system headers, Qt files, external libraries)."));
    allowAccessOutsideProject.setDefaultValue(false);

    autoApplyFileEdits.setSettingsKey(Constants::CA_AUTO_APPLY_FILE_EDITS);
    autoApplyFileEdits.setLabelText(Tr::tr("Automatically apply file edits"));
    autoApplyFileEdits.setToolTip(
        Tr::tr("When enabled, file edits suggested by AI are applied immediately. "
               "When disabled, each edit is staged for manual approval."));
    autoApplyFileEdits.setDefaultValue(false);

    enableListProjectFilesTool.setSettingsKey(Constants::CA_ENABLE_LIST_PROJECT_FILES_TOOL);
    enableListProjectFilesTool.setLabelText(Tr::tr("List Project Files"));
    enableListProjectFilesTool.setToolTip(
        Tr::tr("Lists every source file tracked by the active Qt Creator project(s)."));
    enableListProjectFilesTool.setDefaultValue(true);

    enableFindFileTool.setSettingsKey(Constants::CA_ENABLE_FIND_FILE_TOOL);
    enableFindFileTool.setLabelText(Tr::tr("Find File"));
    enableFindFileTool.setToolTip(
        Tr::tr("Locates a file in the project by name or partial path. Returns paths only, "
               "without file content."));
    enableFindFileTool.setDefaultValue(true);

    enableReadFileTool.setSettingsKey(Constants::CA_ENABLE_READ_FILE_TOOL);
    enableReadFileTool.setLabelText(Tr::tr("Read File"));
    enableReadFileTool.setToolTip(
        Tr::tr("Reads the content of a file by absolute path or path relative to the project root."));
    enableReadFileTool.setDefaultValue(true);

    enableProjectSearchTool.setSettingsKey(Constants::CA_ENABLE_PROJECT_SEARCH_TOOL);
    enableProjectSearchTool.setLabelText(Tr::tr("Search in Project"));
    enableProjectSearchTool.setToolTip(
        Tr::tr("Searches project files for text occurrences or C++ symbol definitions."));
    enableProjectSearchTool.setDefaultValue(true);

    enableCreateNewFileTool.setSettingsKey(Constants::CA_ENABLE_CREATE_NEW_FILE_TOOL);
    enableCreateNewFileTool.setLabelText(Tr::tr("Create New File"));
    enableCreateNewFileTool.setToolTip(
        Tr::tr("Creates a new empty file at the given absolute path, making missing directories."));
    enableCreateNewFileTool.setDefaultValue(true);

    enableEditFileTool.setSettingsKey(Constants::CA_ENABLE_EDIT_FILE_TOOL);
    enableEditFileTool.setLabelText(Tr::tr("Edit File"));
    enableEditFileTool.setToolTip(
        Tr::tr("Applies find-and-replace edits to files. See \"Automatically apply file edits\" "
               "to control whether edits apply immediately or wait for review."));
    enableEditFileTool.setDefaultValue(true);

    enableBuildProjectTool.setSettingsKey(Constants::CA_ENABLE_BUILD_PROJECT_TOOL);
    enableBuildProjectTool.setLabelText(Tr::tr("Build Project"));
    enableBuildProjectTool.setToolTip(
        Tr::tr("Triggers a build of the active Qt Creator project and reports the result."));
    enableBuildProjectTool.setDefaultValue(true);

    enableGetIssuesListTool.setSettingsKey(Constants::CA_ENABLE_GET_ISSUES_LIST_TOOL);
    enableGetIssuesListTool.setLabelText(Tr::tr("Get Issues List"));
    enableGetIssuesListTool.setToolTip(
        Tr::tr("Reads compiler/clang diagnostics from Qt Creator's Issues panel."));
    enableGetIssuesListTool.setDefaultValue(true);

    enableTerminalCommandTool.setSettingsKey(Constants::CA_ENABLE_TERMINAL_COMMAND_TOOL);
    enableTerminalCommandTool.setLabelText(Tr::tr("Execute Terminal Command"));
    enableTerminalCommandTool.setToolTip(
        Tr::tr("Runs a command from the OS-specific allowed list below, in the project directory."));
    enableTerminalCommandTool.setDefaultValue(true);

    enableTodoTool.setSettingsKey(Constants::CA_ENABLE_TODO_TOOL);
    enableTodoTool.setLabelText(Tr::tr("Todo"));
    enableTodoTool.setToolTip(
        Tr::tr("Lets the AI maintain a session-scoped todo list for multi-step workflows."));
    enableTodoTool.setDefaultValue(true);

    allowedTerminalCommandsLinux.setSettingsKey(Constants::CA_ALLOWED_TERMINAL_COMMANDS_LINUX);
    allowedTerminalCommandsLinux.setLabelText(Tr::tr("Allowed Commands (Linux)"));
    allowedTerminalCommandsLinux.setToolTip(
        Tr::tr("Comma-separated list of terminal commands that AI is allowed to execute on Linux. "
               "Example: git, ls, cat, grep, find, cmake"));
    allowedTerminalCommandsLinux.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    allowedTerminalCommandsLinux.setDefaultValue("git, ls, cat, grep, find");

    allowedTerminalCommandsMacOS.setSettingsKey(Constants::CA_ALLOWED_TERMINAL_COMMANDS_MACOS);
    allowedTerminalCommandsMacOS.setLabelText(Tr::tr("Allowed Commands (macOS)"));
    allowedTerminalCommandsMacOS.setToolTip(
        Tr::tr("Comma-separated list of terminal commands that AI is allowed to execute on macOS. "
               "Example: git, ls, cat, grep, find, cmake"));
    allowedTerminalCommandsMacOS.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    allowedTerminalCommandsMacOS.setDefaultValue("git, ls, cat, grep, find");

    allowedTerminalCommandsWindows.setSettingsKey(Constants::CA_ALLOWED_TERMINAL_COMMANDS_WINDOWS);
    allowedTerminalCommandsWindows.setLabelText(Tr::tr("Allowed Commands (Windows)"));
    allowedTerminalCommandsWindows.setToolTip(
        Tr::tr("Comma-separated list of terminal commands that AI is allowed to execute on Windows. "
               "Example: git, dir, type, findstr, where, cmake"));
    allowedTerminalCommandsWindows.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    allowedTerminalCommandsWindows.setDefaultValue("git, dir, type, findstr, where");

    terminalCommandTimeout.setSettingsKey(Constants::CA_TERMINAL_COMMAND_TIMEOUT);
    terminalCommandTimeout.setLabelText(Tr::tr("Command Timeout (seconds)"));
    terminalCommandTimeout.setToolTip(
        Tr::tr("Maximum time in seconds to wait for a terminal command to complete. "
               "Increase for long-running commands like builds."));
    terminalCommandTimeout.setRange(5, 3600);
    terminalCommandTimeout.setDefaultValue(30);

    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    readSettings();

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

#ifdef Q_OS_LINUX
        auto &currentOsCommands = allowedTerminalCommandsLinux;
#elif defined(Q_OS_MACOS)
        auto &currentOsCommands = allowedTerminalCommandsMacOS;
#elif defined(Q_OS_WIN)
        auto &currentOsCommands = allowedTerminalCommandsWindows;
#else
        auto &currentOsCommands = allowedTerminalCommandsLinux; // fallback
#endif

        return Column{
            Row{Stretch{1}, resetToDefaults},
            Space{8},
            Group{
                title(Tr::tr("Tools")),
                Column{
                    enableListProjectFilesTool,
                    enableFindFileTool,
                    enableReadFileTool,
                    enableProjectSearchTool,
                    enableCreateNewFileTool,
                    enableEditFileTool,
                    enableBuildProjectTool,
                    enableGetIssuesListTool,
                    enableTerminalCommandTool,
                    enableTodoTool}},
            Space{8},
            Group{
                title(Tr::tr("Tool Settings")),
                Column{
                    allowAccessOutsideProject,
                    Space{4},
                    Group{
                        title(Tr::tr("Edit File")),
                        Column{autoApplyFileEdits}},
                    Group{
                        title(Tr::tr("Execute Terminal Command")),
                        Column{currentOsCommands, terminalCommandTimeout}}}},
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
        resetAspect(allowAccessOutsideProject);
        resetAspect(autoApplyFileEdits);
        resetAspect(enableListProjectFilesTool);
        resetAspect(enableFindFileTool);
        resetAspect(enableReadFileTool);
        resetAspect(enableProjectSearchTool);
        resetAspect(enableCreateNewFileTool);
        resetAspect(enableEditFileTool);
        resetAspect(enableBuildProjectTool);
        resetAspect(enableGetIssuesListTool);
        resetAspect(enableTerminalCommandTool);
        resetAspect(enableTodoTool);
        resetAspect(allowedTerminalCommandsLinux);
        resetAspect(allowedTerminalCommandsMacOS);
        resetAspect(allowedTerminalCommandsWindows);
        resetAspect(terminalCommandTimeout);
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
