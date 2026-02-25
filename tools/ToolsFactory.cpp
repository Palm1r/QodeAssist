/*
 * Copyright (C) 2025 Petr Mironychev
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

#include "ToolsFactory.hpp"

#include "logger/Logger.hpp"
#include <llmcore/ToolsManager.hpp>
#include <settings/ToolsSettings.hpp>

#include "BuildProjectTool.hpp"
#include "CreateNewFileTool.hpp"
#include "EditFileTool.hpp"
#include "ExecuteTerminalCommandTool.hpp"
#include "FindAndReadFileTool.hpp"
#include "GetIssuesListTool.hpp"
#include "ListProjectFilesTool.hpp"
#include "ProjectSearchTool.hpp"
#include "TodoTool.hpp"

namespace QodeAssist::Tools {

void registerAppTools(LLMCore::ToolsManager *manager)
{
    manager->addTool(new ListProjectFilesTool(manager));
    manager->addTool(new GetIssuesListTool(manager));
    manager->addTool(new CreateNewFileTool(manager));
    manager->addTool(new EditFileTool(manager));
    manager->addTool(new BuildProjectTool(manager));
    manager->addTool(new ExecuteTerminalCommandTool(manager));
    manager->addTool(new ProjectSearchTool(manager));
    manager->addTool(new FindAndReadFileTool(manager));
    manager->addTool(new TodoTool(manager));

    LOG_MESSAGE(QString("Registered %1 tools").arg(manager->registeredTools().size()));
}

void configureToolSettings(LLMCore::ToolsManager *manager)
{
    const auto &settings = Settings::toolsSettings();

    if (auto *t = manager->tool("edit_file"))
        t->setEnabled(settings.enableEditFileTool());
    if (auto *t = manager->tool("build_project"))
        t->setEnabled(settings.enableBuildProjectTool());
    if (auto *t = manager->tool("execute_terminal_command"))
        t->setEnabled(settings.enableTerminalCommandTool());
    if (auto *t = manager->tool("todo_tool"))
        t->setEnabled(settings.enableTodoTool());

    LLMCore::ToolPermissions perms;
    if (settings.allowFileSystemRead())
        perms |= LLMCore::ToolPermission::FileSystemRead;
    if (settings.allowFileSystemWrite())
        perms |= LLMCore::ToolPermission::FileSystemWrite;
    if (settings.allowNetworkAccess())
        perms |= LLMCore::ToolPermission::NetworkAccess;
    manager->setAllowedPermissions(perms);
}

} // namespace QodeAssist::Tools
