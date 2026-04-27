// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
