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

#pragma once

#include <QObject>

#include <pluginllmcore/BaseTool.hpp>

namespace QodeAssist::Tools {

class ToolsFactory : public QObject
{
    Q_OBJECT
public:
    ToolsFactory(QObject *parent = nullptr);
    ~ToolsFactory() override = default;

    QList<PluginLLMCore::BaseTool *> getAvailableTools() const;
    PluginLLMCore::BaseTool *getToolByName(const QString &name) const;
    QJsonArray getToolsDefinitions(
        PluginLLMCore::ToolSchemaFormat format,
        PluginLLMCore::RunToolsFilter filter = PluginLLMCore::RunToolsFilter::ALL) const;
    QString getStringName(const QString &name) const;

private:
    void registerTools();
    void registerTool(PluginLLMCore::BaseTool *tool);

    QHash<QString, PluginLLMCore::BaseTool *> m_tools;
};
} // namespace QodeAssist::Tools
