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

#include "BaseTask.hpp"
#include "TaskPort.hpp"
#include <QUuid>
#include <QtConcurrent>

namespace QodeAssist::TaskFlow {

BaseTask::BaseTask(QObject *parent)
    : QObject(parent)
    , m_taskId("unknown" + QUuid::createUuid().toString())
{}

BaseTask::~BaseTask()
{
    qDeleteAll(m_inputs);
    qDeleteAll(m_outputs);
}

QString BaseTask::taskId() const
{
    return m_taskId;
}

void BaseTask::setTaskId(const QString &taskId)
{
    m_taskId = taskId;
}

QString BaseTask::taskType() const
{
    return QString(metaObject()->className()).split("::").last();
}

void BaseTask::addInputPort(const QString &name)
{
    QMutexLocker locker(&m_tasksMutex);
    m_inputs.append(new TaskPort(name, TaskPort::ValueType::Any, this));
}

void BaseTask::addOutputPort(const QString &name)
{
    QMutexLocker locker(&m_tasksMutex);
    m_outputs.append(new TaskPort(name, TaskPort::ValueType::Any, this));
}

TaskPort *BaseTask::inputPort(const QString &name) const
{
    QMutexLocker locker(&m_tasksMutex);

    auto it = std::find_if(m_inputs.begin(), m_inputs.end(), [&name](const TaskPort *port) {
        return port->name() == name;
    });

    return (it != m_inputs.end()) ? *it : nullptr;
}

TaskPort *BaseTask::outputPort(const QString &name) const
{
    QMutexLocker locker(&m_tasksMutex);

    auto it = std::find_if(m_outputs.begin(), m_outputs.end(), [&name](const TaskPort *port) {
        return port->name() == name;
    });

    return (it != m_outputs.end()) ? *it : nullptr;
}

QList<TaskPort *> BaseTask::getInputPorts() const
{
    QMutexLocker locker(&m_tasksMutex);
    return m_inputs;
}

QList<TaskPort *> BaseTask::getOutputPorts() const
{
    QMutexLocker locker(&m_tasksMutex);
    return m_outputs;
}

QFuture<TaskState> BaseTask::executeAsync()
{
    return QtConcurrent::task([this]() -> TaskState { return execute(); }).spawn();
}

QString BaseTask::taskStateAsString(TaskState state)
{
    switch (state) {
    case TaskState::Success:
        return "Success";
    case TaskState::Failed:
        return "Failed";
    case TaskState::Cancelled:
        return "Cancelled";
    }
    return "Unknown";
}

} // namespace QodeAssist::TaskFlow
