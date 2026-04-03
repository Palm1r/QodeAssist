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

#include <LLMCore/ToolsManager.hpp>

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

void registerQodeAssistTools(::LLMCore::ToolsManager *manager)
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
}

} // namespace QodeAssist::Tools
