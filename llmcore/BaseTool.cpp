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

#include "BaseTool.hpp"

namespace QodeAssist::LLMCore {

BaseTool::BaseTool(QObject *parent)
    : QObject(parent)
{}

QJsonObject BaseTool::customizeForOpenAI(const QJsonObject &baseDefinition) const
{
    QJsonObject function;
    function["name"] = name();
    function["description"] = description();
    function["parameters"] = baseDefinition;

    QJsonObject tool;
    tool["type"] = "function";
    tool["function"] = function;

    return tool;
}

QJsonObject BaseTool::customizeForClaude(const QJsonObject &baseDefinition) const
{
    QJsonObject tool;
    tool["name"] = name();
    tool["description"] = description();
    tool["input_schema"] = baseDefinition;

    return tool;
}

QJsonObject BaseTool::customizeForOllama(const QJsonObject &baseDefinition) const
{
    return customizeForOpenAI(baseDefinition);
}

} // namespace QodeAssist::LLMCore
