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
#include <logger/Logger.hpp>
#include <QtConcurrent>

#include "tasks/TaskPort.hpp"

namespace QodeAssist {

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

void BaseTask::addInputPort(const QString &name)
{
    QMutexLocker locker(&m_tasksMutex);
    m_inputs.append(new TaskPort(name));
}

void BaseTask::addOutputPort(const QString &name)
{
    QMutexLocker locker(&m_tasksMutex);
    m_outputs.append(new TaskPort(name));
}

TaskPort *BaseTask::getInputPort(const QString &name) const
{
    for (TaskPort *port : m_inputs) {
        if (port->name() == name) {
            return port;
        }
    }
    return nullptr;
}

TaskPort *BaseTask::getOutputPort(const QString &name) const
{
    for (TaskPort *port : m_outputs) {
        if (port->name() == name) {
            return port;
        }
    }
    return nullptr;
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

QVariant BaseTask::getInputValue(const QString &portName) const
{
    QMutexLocker locker(&m_tasksMutex);
    TaskPort *port = getInputPort(portName);
    if (port) {
        return port->getConnectedValue();
    }
    return QVariant();
}

void BaseTask::setOutputValue(const QString &portName, const QVariant &value)
{
    QMutexLocker locker(&m_tasksMutex);
    TaskPort *port = getOutputPort(portName);
    if (port) {
        port->setValue(value);
    }
}

void BaseTask::setPortConnection(TaskPort *port, TaskConnection *connection)
{
    if (port) {
        port->setConnection(connection);
    }
}

QFuture<TaskState> BaseTask::executeAsync()
{
    return QtConcurrent::run([this]() -> TaskState {
        LOG_MESSAGE(QString("Task '%1' started").arg(m_taskId));

        try {
            TaskState result = execute();
            return result;
        } catch (...) {
            return TaskState::Failed;
        }
    });
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
    default:
        return QString("Unknown(%1)").arg(static_cast<int>(state));
    }
}

QJsonObject BaseTask::toJson() const
{
    QJsonObject taskObj;

    taskObj["taskId"] = m_taskId;
    taskObj["taskType"] = getTaskType();

    QJsonObject params;
    QMutexLocker locker(&m_tasksMutex);
    for (auto it = m_parameters.constBegin(); it != m_parameters.constEnd(); ++it) {
        params[it.key()] = QJsonValue::fromVariant(it.value());
    }
    if (!params.isEmpty())
        taskObj["params"] = params;

    return taskObj;
}

bool BaseTask::fromJson(const QJsonObject &json)
{
    LOG_MESSAGE(
        QString("BaseTask::fromJson called with: %1").arg(QString(QJsonDocument(json).toJson())));

    if (json.contains("taskId")) {
        m_taskId = json["taskId"].toString();
        LOG_MESSAGE(QString("BaseTask::fromJson - set taskId to: %1").arg(m_taskId));
    }

    if (json.contains("params")) {
        QJsonObject params = json["params"].toObject();
        QMutexLocker locker(&m_tasksMutex);
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            m_parameters[it.key()] = it.value().toVariant();
        }
        LOG_MESSAGE(QString("BaseTask::fromJson - loaded %1 parameters").arg(m_parameters.size()));
    }

    return json.contains("taskId");
}

QString BaseTask::getTaskType() const
{
    return QString(metaObject()->className()).split("::").last();
}

void BaseTask::addParameter(const QString &name, QVariant defaultValue)
{
    QMutexLocker locker(&m_tasksMutex);
    if (!m_parameters.contains(name)) {
        m_parameters.insert(name, defaultValue);
    }
}

QVariant BaseTask::getParameter(const QString &name) const
{
    QMutexLocker locker(&m_tasksMutex);
    if (m_parameters.contains(name)) {
        return m_parameters.value(name);
    }
    LOG_MESSAGE(QString("BaseTask::getParameter - parameter '%1' not found").arg(name));
    return QVariant();
}

void BaseTask::setParameter(const QString &name, const QVariant &value)
{
    QMutexLocker locker(&m_tasksMutex);
    m_parameters[name] = value;
    if (!m_parameters.contains(name)) {
        LOG_MESSAGE(QString("BaseTask::setParameter - added new parameter '%1' with value: %2")
                        .arg(name, value.toString()));
    }
}

} // namespace QodeAssist
