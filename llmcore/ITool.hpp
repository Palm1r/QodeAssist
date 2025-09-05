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

#include <QJsonObject>
#include <QString>

namespace QodeAssist::LLMCore {

class ITool
{
public:
    virtual ~ITool() = default;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QJsonObject getDefinition() const = 0;
    virtual QString execute(const QJsonObject &input = QJsonObject()) = 0;
};

} // namespace QodeAssist::LLMCore
