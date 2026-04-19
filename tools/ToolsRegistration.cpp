/*
 * Copyright (C) 2026 Petr Mironychev
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

#include "ToolsRegistration.hpp"

#include <LLMQore/ToolsManager.hpp>

#include <settings/ToolsSettings.hpp>
#include <utils/aspects.h>

#include "BuildProjectTool.hpp"
#include "CreateNewFileTool.hpp"
#include "EditFileTool.hpp"
#include "ExecuteTerminalCommandTool.hpp"
#include "FindFileTool.hpp"
#include "GetIssuesListTool.hpp"
#include "ListProjectFilesTool.hpp"
#include "ProjectSearchTool.hpp"
#include "ReadFileTool.hpp"
#include "TodoTool.hpp"

namespace QodeAssist::Tools {

namespace {

template<typename ToolT>
void wireTool(::LLMQore::ToolsManager *manager,
              Utils::BoolAspect &aspect,
              const QString &toolId)
{
    auto sync = [manager, toolId, &aspect]() {
        const bool wanted = aspect.volatileValue();
        const bool present = manager->tool(toolId) != nullptr;
        if (wanted && !present) {
            manager->addTool(new ToolT(manager));
        } else if (!wanted && present) {
            manager->removeTool(toolId);
        }
    };

    sync();

    QObject::connect(&aspect, &Utils::BoolAspect::volatileValueChanged, manager, sync);
    QObject::connect(&aspect, &Utils::BaseAspect::changed, manager, sync);
}

} // namespace

void registerQodeAssistTools(::LLMQore::ToolsManager *manager)
{
    auto &s = Settings::toolsSettings();

    wireTool<ListProjectFilesTool>(manager, s.enableListProjectFilesTool, "list_project_files");
    wireTool<FindFileTool>(manager, s.enableFindFileTool, "find_file");
    wireTool<ReadFileTool>(manager, s.enableReadFileTool, "read_file");
    wireTool<ProjectSearchTool>(manager, s.enableProjectSearchTool, "search_project");
    wireTool<CreateNewFileTool>(manager, s.enableCreateNewFileTool, "create_new_file");
    wireTool<EditFileTool>(manager, s.enableEditFileTool, "edit_file");
    wireTool<BuildProjectTool>(manager, s.enableBuildProjectTool, "build_project");
    wireTool<GetIssuesListTool>(manager, s.enableGetIssuesListTool, "get_issues_list");
    wireTool<ExecuteTerminalCommandTool>(
        manager, s.enableTerminalCommandTool, "execute_terminal_command");
    wireTool<TodoTool>(manager, s.enableTodoTool, "todo_tool");
}

} // namespace QodeAssist::Tools
