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

#include <functional>
#include <QHash>
#include <QString>

namespace QodeAssist {

class BaseTask;

class TaskRegistry
{
public:
    using TaskCreator = std::function<BaseTask *()>;

    TaskRegistry();

    template<typename T>
    void registerTask(const QString &taskType)
    {
        m_creators[taskType] = []() -> BaseTask * { return new T(); };
    }

    BaseTask *createTask(const QString &taskType) const;

    static TaskRegistry createWithDefaults();

    QStringList getAvailableTypes() const;

private:
    QHash<QString, TaskCreator> m_creators;
};

} // namespace QodeAssist
