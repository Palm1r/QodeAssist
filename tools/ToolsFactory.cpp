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
#include <settings/GeneralSettings.hpp>
#include <settings/ToolsSettings.hpp>
#include <QJsonArray>
#include <QJsonObject>

#include "BuildProjectTool.hpp"
#include "CreateNewFileTool.hpp"
#include "EditFileTool.hpp"
#include "FindAndReadFileTool.hpp"
#include "GetIssuesListTool.hpp"
#include "ListProjectFilesTool.hpp"
#include "ProjectSearchTool.hpp"
#include "ReadVisibleFilesTool.hpp"

namespace QodeAssist::Tools {

ToolsFactory::ToolsFactory(QObject *parent)
    : QObject(parent)
{
    registerTools();
}

void ToolsFactory::registerTools()
{
    registerTool(new ReadVisibleFilesTool(this));
    registerTool(new ListProjectFilesTool(this));
    registerTool(new GetIssuesListTool(this));
    registerTool(new CreateNewFileTool(this));
    registerTool(new EditFileTool(this));
    registerTool(new BuildProjectTool(this));
    registerTool(new ProjectSearchTool(this));
    registerTool(new FindAndReadFileTool(this));

    LOG_MESSAGE(QString("Registered %1 tools").arg(m_tools.size()));
}

void ToolsFactory::registerTool(LLMCore::BaseTool *tool)
{
    if (!tool) {
        LOG_MESSAGE("Warning: Attempted to register null tool");
        return;
    }

    const QString toolName = tool->name();
    if (m_tools.contains(toolName)) {
        LOG_MESSAGE(QString("Warning: Tool '%1' already registered, replacing").arg(toolName));
    }

    m_tools.insert(toolName, tool);
}

QList<LLMCore::BaseTool *> ToolsFactory::getAvailableTools() const
{
    return m_tools.values();
}

LLMCore::BaseTool *ToolsFactory::getToolByName(const QString &name) const
{
    return m_tools.value(name, nullptr);
}

QJsonArray ToolsFactory::getToolsDefinitions(LLMCore::ToolSchemaFormat format) const
{
    QJsonArray toolsArray;
    const auto &settings = Settings::toolsSettings();

    for (auto it = m_tools.constBegin(); it != m_tools.constEnd(); ++it) {
        if (!it.value()) {
            continue;
        }

        if (it.value()->name() == "edit_file" && !settings.enableEditFileTool()) {
            continue;
        }

        if (it.value()->name() == "build_project" && !settings.enableBuildProjectTool()) {
            continue;
        }

        const auto requiredPerms = it.value()->requiredPermissions();
        bool hasPermission = true;

        if (requiredPerms.testFlag(LLMCore::ToolPermission::FileSystemRead)) {
            if (!settings.allowFileSystemRead()) {
                hasPermission = false;
            }
        }

        if (requiredPerms.testFlag(LLMCore::ToolPermission::FileSystemWrite)) {
            if (!settings.allowFileSystemWrite()) {
                hasPermission = false;
            }
        }

        // if (requiredPerms.testFlag(LLMCore::ToolPermission::NetworkAccess)) {
        //     if (!settings.allowNetworkAccess()) {
        //         hasPermission = false;
        //     }
        // }

        if (hasPermission) {
            toolsArray.append(it.value()->getDefinition(format));
        } else {
            LOG_MESSAGE(
                QString("Tool '%1' skipped due to missing permissions").arg(it.value()->name()));
        }
    }

    return toolsArray;
}

QString ToolsFactory::getStringName(const QString &name) const
{
    return m_tools.contains(name) ? m_tools.value(name)->stringName() : QString("Unknown tools");
}

} // namespace QodeAssist::Tools
