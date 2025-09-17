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

#include <QJsonArray>
#include <QList>

#include "ITool.hpp"

namespace QodeAssist::LLMCore {

class IToolsFactory
{
public:
    virtual ~IToolsFactory() = default;
    virtual QList<ITool *> getAvailableTools() const = 0;
    virtual ITool *getToolByName(const QString &name) const = 0;
    virtual QJsonArray getToolsDefinitions() const = 0;
};

} // namespace QodeAssist::LLMCore
