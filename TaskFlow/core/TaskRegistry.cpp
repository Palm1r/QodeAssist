// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
