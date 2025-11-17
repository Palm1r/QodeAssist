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

#include <llmcore/BaseTool.hpp>

namespace QodeAssist::Tools {

class ToolsFactory : public QObject
{
    Q_OBJECT
public:
    ToolsFactory(QObject *parent = nullptr);
    ~ToolsFactory() override = default;

    QList<LLMCore::BaseTool *> getAvailableTools() const;
    LLMCore::BaseTool *getToolByName(const QString &name) const;
    QJsonArray getToolsDefinitions(
        LLMCore::ToolSchemaFormat format,
        LLMCore::RunToolsFilter filter = LLMCore::RunToolsFilter::ALL) const;
    QString getStringName(const QString &name) const;

private:
    void registerTools();
    void registerTool(LLMCore::BaseTool *tool);

    QHash<QString, LLMCore::BaseTool *> m_tools;
};
} // namespace QodeAssist::Tools
