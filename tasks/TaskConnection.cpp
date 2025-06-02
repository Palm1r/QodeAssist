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

#include <QJsonObject>
#include <QStringList>

#include "BaseTask.hpp"
#include "TaskPort.hpp"

namespace QodeAssist {

bool TaskConnection::operator==(const TaskConnection &other) const
{
    return sourceTask == other.sourceTask && targetTask == other.targetTask
           && sourcePort == other.sourcePort && targetPort == other.targetPort;
}

QString TaskConnection::toString() const
{
    if (!sourceTask || !targetTask || !sourcePort || !targetPort) {
        return QString();
    }

    return QString("%1.%2->%3.%4")
        .arg(sourceTask->taskId(), sourcePort->name(), targetTask->taskId(), targetPort->name());
}

TaskConnection *TaskConnection::fromString(
    const QString &connectionStr, const QHash<QString, BaseTask *> &tasks)
{
    QStringList parts = connectionStr.split("->");
    if (parts.size() != 2) {
        return nullptr;
    }

    QStringList sourceParts = parts[0].split(".");
    QStringList targetParts = parts[1].split(".");
    if (sourceParts.size() != 2 || targetParts.size() != 2) {
        return nullptr;
    }

    QString sourceTaskId = sourceParts[0];
    QString sourcePortName = sourceParts[1];
    QString targetTaskId = targetParts[0];
    QString targetPortName = targetParts[1];

    BaseTask *sourceTask = tasks.value(sourceTaskId);
    BaseTask *targetTask = tasks.value(targetTaskId);

    if (!sourceTask || !targetTask) {
        return nullptr;
    }

    TaskPort *sourcePort = sourceTask->getOutputPort(sourcePortName);
    TaskPort *targetPort = targetTask->getInputPort(targetPortName);

    if (!sourcePort || !targetPort) {
        return nullptr;
    }

    TaskConnection *connection = new TaskConnection;
    connection->sourceTask = sourceTask;
    connection->targetTask = targetTask;
    connection->sourcePort = sourcePort;
    connection->targetPort = targetPort;

    return connection;
}

QJsonObject TaskConnection::toJson() const
{
    QJsonObject connObj;

    if (sourceTask && targetTask && sourcePort && targetPort) {
        connObj["sourceTask"] = sourceTask->taskId();
        connObj["sourcePort"] = sourcePort->name();
        connObj["targetTask"] = targetTask->taskId();
        connObj["targetPort"] = targetPort->name();
        connObj["connectionString"] = toString();
    }

    return connObj;
}

TaskConnection *TaskConnection::fromJson(
    const QJsonObject &json, const QHash<QString, BaseTask *> &tasks)
{
    QString sourceTaskId = json["sourceTask"].toString();
    QString sourcePortName = json["sourcePort"].toString();
    QString targetTaskId = json["targetTask"].toString();
    QString targetPortName = json["targetPort"].toString();

    BaseTask *sourceTask = tasks.value(sourceTaskId);
    BaseTask *targetTask = tasks.value(targetTaskId);

    if (!sourceTask || !targetTask) {
        return nullptr;
    }

    TaskPort *sourcePort = sourceTask->getOutputPort(sourcePortName);
    TaskPort *targetPort = targetTask->getInputPort(targetPortName);

    if (!sourcePort || !targetPort) {
        return nullptr;
    }

    TaskConnection *connection = new TaskConnection;
    connection->sourceTask = sourceTask;
    connection->targetTask = targetTask;
    connection->sourcePort = sourcePort;
    connection->targetPort = targetPort;

    return connection;
}

} // namespace QodeAssist
