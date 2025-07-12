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

#include "TaskRegistry.hpp"

#include <Logger.hpp>

#include "BaseTask.hpp"

namespace QodeAssist::TaskFlow {

TaskRegistry::TaskRegistry(QObject *parent)
    : QObject(parent)
{}

BaseTask *TaskRegistry::createTask(const QString &taskType, QObject *parent) const
{
    LOG_MESSAGE(QString("Trying to create task: %1").arg(taskType));

    if (m_creators.contains(taskType)) {
        LOG_MESSAGE(QString("Found creator for task type: %1").arg(taskType));
        try {
            BaseTask *task = m_creators[taskType](parent);
            if (task) {
                LOG_MESSAGE(QString("Successfully created task: %1").arg(taskType));
                return task;
            }
        } catch (...) {
            LOG_MESSAGE(QString("Exception while creating task of type: %1").arg(taskType));
        }
    } else {
        LOG_MESSAGE(QString("No creator found for task type: %1").arg(taskType));
    }

    return nullptr;
}

QStringList TaskRegistry::getAvailableTypes() const
{
    return m_creators.keys();
}

} // namespace QodeAssist::TaskFlow
