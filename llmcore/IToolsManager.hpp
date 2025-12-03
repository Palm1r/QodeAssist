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

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "BaseTool.hpp"

namespace QodeAssist::LLMCore {

class IToolsManager
{
public:
    virtual ~IToolsManager() = default;

    virtual void executeToolCall(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QJsonObject &input) = 0;

    virtual QJsonArray getToolsDefinitions(
        ToolSchemaFormat format,
        RunToolsFilter filter = RunToolsFilter::ALL) const = 0;

    virtual void cleanupRequest(const QString &requestId) = 0;
    virtual void setCurrentSessionId(const QString &sessionId) = 0;
    virtual void clearTodoSession(const QString &sessionId) = 0;
};

} // namespace QodeAssist::LLMCore
