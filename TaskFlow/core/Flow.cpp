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

#include "Flow.hpp"
#include "TaskPort.hpp"
#include <QUuid>
#include <QtConcurrent>

namespace QodeAssist::TaskFlow {

Flow::Flow(QObject *parent)
    : QObject(parent)
    , m_flowId("flow_" + QUuid::createUuid().toString())
{}

Flow::~Flow()
{
    QMutexLocker locker(&m_flowMutex);
    qDeleteAll(m_connections);
    qDeleteAll(m_tasks);
}

QString Flow::flowId() const
{
    return m_flowId;
}

void Flow::setFlowId(const QString &flowId)
{
    if (m_flowId != flowId) {
        m_flowId = flowId;
    }
}

void Flow::addTask(BaseTask *task)
{
    if (!task) {
        return;
    }

    QMutexLocker locker(&m_flowMutex);

    QString taskId = task->taskId();
    if (m_tasks.contains(taskId)) {
        qWarning() << "Flow::addTask - Task with ID" << taskId << "already exists";
        return;
    }

    m_tasks.insert(taskId, task);
    task->setParent(this);

    emit taskAdded(taskId);
}

void Flow::removeTask(const QString &taskId)
{
    QMutexLocker locker(&m_flowMutex);

    BaseTask *task = m_tasks.value(taskId);
    if (!task) {
        return;
    }

    auto it = m_connections.begin();
    while (it != m_connections.end()) {
        TaskConnection *connection = *it;
        if (connection->sourceTask() == task || connection->targetTask() == task) {
            it = m_connections.erase(it);
            emit connectionRemoved(connection);
            delete connection;
        } else {
            ++it;
        }
    }

    m_tasks.remove(taskId);
    emit taskRemoved(taskId);
    delete task;
}

void Flow::removeTask(BaseTask *task)
{
    if (!task) {
        return;
    }
    removeTask(task->taskId());
}

BaseTask *Flow::getTask(const QString &taskId) const
{
    QMutexLocker locker(&m_flowMutex);
    return m_tasks.value(taskId);
}

bool Flow::hasTask(const QString &taskId) const
{
    QMutexLocker locker(&m_flowMutex);
    return m_tasks.contains(taskId);
}

QHash<QString, BaseTask *> Flow::tasks() const
{
    QMutexLocker locker(&m_flowMutex);
    return m_tasks;
}

TaskConnection *Flow::addConnection(TaskPort *sourcePort, TaskPort *targetPort)
{
    if (!sourcePort || !targetPort) {
        qWarning() << "Flow::addConnection - Invalid ports";
        return nullptr;
    }

    // Verify ports belong to tasks in this flow
    BaseTask *sourceTask = qobject_cast<BaseTask *>(sourcePort->parent());
    BaseTask *targetTask = qobject_cast<BaseTask *>(targetPort->parent());

    if (!sourceTask || !targetTask) {
        qWarning() << "Flow::addConnection - Ports don't belong to valid tasks";
        return nullptr;
    }

    QMutexLocker locker(&m_flowMutex);

    if (!m_tasks.contains(sourceTask->taskId()) || !m_tasks.contains(targetTask->taskId())) {
        qWarning() << "Flow::addConnection - Tasks not in this flow";
        return nullptr;
    }

    for (TaskConnection *existingConnection : m_connections) {
        if (existingConnection->sourcePort() == sourcePort
            && existingConnection->targetPort() == targetPort) {
            qWarning() << "Flow::addConnection - Connection already exists";
            return existingConnection;
        }
    }

    TaskConnection *connection = new TaskConnection(sourcePort, targetPort, this);
    m_connections.append(connection);

    emit connectionAdded(connection);
    return connection;
}

void Flow::removeConnection(TaskConnection *connection)
{
    if (!connection) {
        return;
    }

    QMutexLocker locker(&m_flowMutex);

    if (m_connections.removeOne(connection)) {
        emit connectionRemoved(connection);
        delete connection;
    }
}

QList<TaskConnection *> Flow::connections() const
{
    QMutexLocker locker(&m_flowMutex);
    return m_connections;
}

QFuture<FlowState> Flow::executeAsync()
{
    return QtConcurrent::run([this]() { return execute(); });
}

FlowState Flow::execute()
{
    emit executionStarted();

    if (!isValid()) {
        emit executionFinished(FlowState::Failed);
        return FlowState::Failed;
    }

    if (hasCircularDependencies()) {
        qWarning() << "Flow::execute - Circular dependencies detected";
        emit executionFinished(FlowState::Failed);
        return FlowState::Failed;
    }

    QList<BaseTask *> executionOrder = getExecutionOrder();

    for (BaseTask *task : executionOrder) {
        TaskState taskResult = task->execute();

        if (taskResult == TaskState::Failed) {
            qWarning() << "Flow::execute - Task" << task->taskId() << "failed";
            emit executionFinished(FlowState::Failed);
            return FlowState::Failed;
        }

        if (taskResult == TaskState::Cancelled) {
            qWarning() << "Flow::execute - Task" << task->taskId() << "cancelled";
            emit executionFinished(FlowState::Cancelled);
            return FlowState::Cancelled;
        }
    }

    emit executionFinished(FlowState::Success);
    return FlowState::Success;
}

bool Flow::isValid() const
{
    QMutexLocker locker(&m_flowMutex);

    // Check all connections are valid
    for (TaskConnection *connection : m_connections) {
        if (!connection->isValid()) {
            return false;
        }
    }

    return true;
}

bool Flow::hasCircularDependencies() const
{
    return detectCircularDependencies();
}

QString Flow::flowStateAsString(FlowState state)
{
    switch (state) {
    case FlowState::Success:
        return "Success";
    case FlowState::Failed:
        return "Failed";
    case FlowState::Cancelled:
        return "Cancelled";
    }
    return "Unknown";
}

QStringList Flow::getTaskIds() const
{
    QMutexLocker locker(&m_flowMutex);
    return m_tasks.keys();
}

QList<BaseTask *> Flow::getExecutionOrder() const
{
    QMutexLocker locker(&m_flowMutex);

    QList<BaseTask *> result;
    QSet<BaseTask *> visited;
    QList<BaseTask *> allTasks = m_tasks.values();

    std::function<void(BaseTask *)> visit = [&](BaseTask *task) {
        if (visited.contains(task)) {
            return;
        }

        visited.insert(task);

        QList<BaseTask *> dependencies = getTaskDependencies(task);
        for (BaseTask *dependency : dependencies) {
            visit(dependency);
        }

        result.append(task);
    };

    for (BaseTask *task : allTasks) {
        visit(task);
    }

    return result;
}

bool Flow::detectCircularDependencies() const
{
    QMutexLocker locker(&m_flowMutex);

    QSet<BaseTask *> visited;
    QSet<BaseTask *> recursionStack;
    bool hasCycle = false;

    for (BaseTask *task : m_tasks.values()) {
        if (!visited.contains(task)) {
            visitTask(task, visited, recursionStack, hasCycle);
            if (hasCycle) {
                return true;
            }
        }
    }

    return false;
}

void Flow::visitTask(
    BaseTask *task, QSet<BaseTask *> &visited, QSet<BaseTask *> &recursionStack, bool &hasCycle) const
{
    if (hasCycle) {
        return;
    }

    visited.insert(task);
    recursionStack.insert(task);

    for (TaskConnection *connection : m_connections) {
        if (connection->sourceTask() == task) {
            BaseTask *dependentTask = connection->targetTask();

            if (recursionStack.contains(dependentTask)) {
                hasCycle = true;
                return;
            }

            if (!visited.contains(dependentTask)) {
                visitTask(dependentTask, visited, recursionStack, hasCycle);
            }
        }
    }

    recursionStack.remove(task);
}

QList<BaseTask *> Flow::getTaskDependencies(BaseTask *task) const
{
    QList<BaseTask *> dependencies;

    for (TaskConnection *connection : m_connections) {
        if (connection->targetTask() == task) {
            BaseTask *dependencyTask = connection->sourceTask();
            if (!dependencies.contains(dependencyTask)) {
                dependencies.append(dependencyTask);
            }
        }
    }

    return dependencies;
}

} // namespace QodeAssist::TaskFlow
