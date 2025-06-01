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
#include "RegisterTasksList.hpp"
#include "TaskPort.hpp"
#include <logger/Logger.hpp>
#include <QtConcurrent>

namespace QodeAssist {

Flow::Flow(const QString &flowId, QObject *parent)
    : QObject(parent)
    , m_flowId(flowId)
{
    LOG_MESSAGE(QString("Flow created with ID: %1").arg(flowId));
    registerFlowTasks();
}

Flow::~Flow()
{
    qDeleteAll(m_connections);
}

QString Flow::flowId() const
{
    return m_flowId;
}

void Flow::addTask(BaseTask *task)
{
    if (!task)
        return;

    m_tasks.insert(task->taskId(), task);
    task->setParent(this);
}

void Flow::addConnection(
    BaseTask *sourceTask, TaskPort *sourcePort, BaseTask *targetTask, TaskPort *targetPort)
{
    if (!sourceTask || !targetTask || !sourcePort || !targetPort) {
        return;
    }

    TaskConnection *connection = new TaskConnection;
    connection->sourceTask = sourceTask;
    connection->targetTask = targetTask;
    connection->sourcePort = sourcePort;
    connection->targetPort = targetPort;

    sourceTask->setPortConnection(sourcePort, connection);
    targetTask->setPortConnection(targetPort, connection);

    m_connections.append(connection);
}

QFuture<FlowState> Flow::executeAsync()
{
    return QtConcurrent::run([this]() -> FlowState {
        LOG_MESSAGE(QString("Flow '%1' started").arg(m_flowId));

        try {
            FlowState result = execute();
            LOG_MESSAGE(QString("Flow '%1' finished with state: %2")
                            .arg(m_flowId, flowStateAsString(result)));
            return result;
        } catch (...) {
            LOG_MESSAGE(QString("Flow '%1' failed with exception").arg(m_flowId));
            return FlowState::Failed;
        }
    });
}

FlowState Flow::execute()
{
    if (m_tasks.isEmpty()) {
        return FlowState::Failed;
    }

    QList<BaseTask *> executedTasks;
    QList<BaseTask *> remainingTasks = m_tasks.values();

    while (!remainingTasks.isEmpty()) {
        bool progressMade = false;

        for (auto it = remainingTasks.begin(); it != remainingTasks.end();) {
            BaseTask *task = *it;

            bool canExecute = true;
            for (TaskPort *inputPort : task->getInputPorts()) {
                if (inputPort->hasConnection()) {
                    TaskConnection *conn = inputPort->connection();
                    if (!executedTasks.contains(conn->sourceTask)) {
                        canExecute = false;
                        break;
                    }
                }
            }

            if (canExecute) {
                LOG_MESSAGE(QString("Executing task: %1").arg(task->taskId()));
                TaskState taskResult = task->execute();

                if (taskResult != TaskState::Success) {
                    return FlowState::Failed;
                }

                executedTasks.append(task);
                it = remainingTasks.erase(it);
                progressMade = true;
            } else {
                ++it;
            }
        }

        if (!progressMade) {
            LOG_MESSAGE("Flow deadlock detected - circular dependencies or missing tasks");
            return FlowState::Failed;
        }
    }

    return FlowState::Success;
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
    default:
        return QString("Unknown(%1)").arg(static_cast<int>(state));
    }
}

QJsonObject Flow::toJson() const
{
    QJsonObject flowObj;
    flowObj["flowId"] = m_flowId;

    QJsonArray tasksArray;
    for (auto it = m_tasks.constBegin(); it != m_tasks.constEnd(); ++it) {
        BaseTask *task = it.value();
        tasksArray.append(task->toJson());
    }
    flowObj["tasks"] = tasksArray;

    QJsonArray connectionsArray;
    for (TaskConnection *conn : m_connections) {
        connectionsArray.append(conn->toString());
    }
    flowObj["connections"] = connectionsArray;

    return flowObj;
}

bool Flow::fromJson(const QJsonObject &json)
{
    m_tasks.clear();
    qDeleteAll(m_connections);
    m_connections.clear();

    m_flowId = json["flowId"].toString();

    QJsonArray tasksArray = json["tasks"].toArray();
    for (const QJsonValue &taskValue : tasksArray) {
        QJsonObject taskObj = taskValue.toObject();

        BaseTask *task = createTaskFromObject(taskObj);
        if (task) {
            addTask(task);
        } else {
            QString taskType = taskObj["taskType"].toString();
            LOG_MESSAGE(QString("Failed to create task of type: %1").arg(taskType));
            return false;
        }
    }

    QJsonArray connectionsArray = json["connections"].toArray();
    for (const QJsonValue &connValue : connectionsArray) {
        QString connStr = connValue.toString();

        TaskConnection *connection = TaskConnection::fromString(connStr, m_tasks);
        if (connection) {
            connection->sourcePort->setConnection(connection);
            connection->targetPort->setConnection(connection);
            m_connections.append(connection);
        }
    }

    return true;
}

QByteArray Flow::toJsonData() const
{
    QJsonDocument doc(toJson());
    return doc.toJson();
}

bool Flow::fromJsonData(const QByteArray &jsonData)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);

    if (error.error != QJsonParseError::NoError) {
        LOG_MESSAGE(QString("JSON parse error: %1").arg(error.errorString()));
        return false;
    }

    return fromJson(doc.object());
}

BaseTask *Flow::createTaskByType(
    const QString &taskType, const QString &taskId, const QJsonObject &params)
{
    QJsonObject taskObj;
    taskObj["taskType"] = taskType;
    taskObj["taskId"] = taskId;
    taskObj["params"] = params;

    return createTaskFromObject(taskObj);
}

BaseTask *Flow::createTaskFromObject(const QJsonObject &taskObj)
{
    QString taskType = taskObj["taskType"].toString();

    if (taskType.isEmpty()) {
        LOG_MESSAGE("Flow::createTaskFromObject - missing taskType");
        return nullptr;
    }

    BaseTask *task = m_registry.createTask(taskType);

    if (task) {
        if (task->fromJson(taskObj)) {
            LOG_MESSAGE(QString("Flow::createTaskFromObject - created %1 with ID: %2")
                            .arg(taskType, task->taskId()));
            return task;
        } else {
            LOG_MESSAGE(
                QString("Flow::createTaskFromObject - fromJson failed for %1").arg(taskType));
            delete task;
            return nullptr;
        }
    }

    LOG_MESSAGE(
        QString("Flow::createTaskFromObject - failed to create task type: %1").arg(taskType));

    return nullptr;
}

QStringList Flow::getAvailableTaskTypes() const
{
    return m_registry.getAvailableTypes();
}

void Flow::registerFlowTasks()
{
    LOG_MESSAGE("Registering all tasks...");
    registerTasksList(m_registry);
    LOG_MESSAGE("All tasks registered successfully");
}

} // namespace QodeAssist
