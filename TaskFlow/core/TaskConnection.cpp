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

#include "TaskConnection.hpp"
#include "BaseTask.hpp"
#include "TaskPort.hpp"
#include <QMetaEnum>

namespace QodeAssist::TaskFlow {

TaskConnection::TaskConnection(TaskPort *sourcePort, TaskPort *targetPort, QObject *parent)
    : QObject(parent)
    , m_sourcePort(sourcePort)
    , m_targetPort(targetPort)
{
    setupConnection();
}

TaskConnection::~TaskConnection()
{
    cleanupConnection();
}

BaseTask *TaskConnection::sourceTask() const
{
    return m_sourcePort ? qobject_cast<BaseTask *>(m_sourcePort->parent()) : nullptr;
}

BaseTask *TaskConnection::targetTask() const
{
    return m_targetPort ? qobject_cast<BaseTask *>(m_targetPort->parent()) : nullptr;
}

TaskPort *TaskConnection::sourcePort() const
{
    return m_sourcePort;
}

TaskPort *TaskConnection::targetPort() const
{
    return m_targetPort;
}

bool TaskConnection::isValid() const
{
    return m_sourcePort && m_targetPort && m_sourcePort != m_targetPort && sourceTask()
           && targetTask() && sourceTask() != targetTask();
}

bool TaskConnection::isTypeCompatible() const
{
    if (!isValid()) {
        return false;
    }

    return m_targetPort->isConnectionTypeCompatible(m_sourcePort);
}

QString TaskConnection::toString() const
{
    if (!isValid()) {
        return QString();
    }

    BaseTask *srcTask = sourceTask();
    BaseTask *tgtTask = targetTask();

    return QString("%1.%2->%3.%4")
        .arg(srcTask->taskId())
        .arg(m_sourcePort->name())
        .arg(tgtTask->taskId())
        .arg(m_targetPort->name());
}

bool TaskConnection::operator==(const TaskConnection &other) const
{
    return m_sourcePort == other.m_sourcePort && m_targetPort == other.m_targetPort;
}

void TaskConnection::setupConnection()
{
    if (!isValid()) {
        qWarning() << "TaskConnection::setupConnection - Invalid connection parameters";
        return;
    }

    if (!isTypeCompatible()) {
        QMetaEnum metaEnum = QMetaEnum::fromType<TaskPort::ValueType>();
        qWarning() << "TaskConnection::setupConnection - Type incompatible connection:"
                   << metaEnum.valueToKey(static_cast<int>(m_sourcePort->valueType())) << "to"
                   << metaEnum.valueToKey(static_cast<int>(m_targetPort->valueType()));
    }

    m_sourcePort->setConnection(this);
    m_targetPort->setConnection(this);
}

void TaskConnection::cleanupConnection()
{
    if (m_sourcePort && m_sourcePort->connection() == this) {
        m_sourcePort->setConnection(nullptr);
    }

    if (m_targetPort && m_targetPort->connection() == this) {
        m_targetPort->setConnection(nullptr);
    }
}

} // namespace QodeAssist::TaskFlow
