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

#include <QFuture>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QVariantMap>

namespace QodeAssist {

enum class TaskState { Success, Failed, Cancelled };

class TaskConnection;
class TaskPort;

class BaseTask : public QObject
{
    Q_OBJECT

public:
    explicit BaseTask(QObject *parent = nullptr);
    ~BaseTask() override;

    QString taskId() const;
    void setTaskId(const QString &taskId);

    void addInputPort(const QString &name);
    void addOutputPort(const QString &name);

    TaskPort *getInputPort(const QString &name) const;
    TaskPort *getOutputPort(const QString &name) const;
    QList<TaskPort *> getInputPorts() const;
    QList<TaskPort *> getOutputPorts() const;

    QVariant getInputValue(const QString &portName) const;
    void setOutputValue(const QString &portName, const QVariant &value);

    void setPortConnection(TaskPort *port, TaskConnection *connection);

    QFuture<TaskState> executeAsync();
    virtual TaskState execute() = 0;

    static QString taskStateAsString(TaskState state);

    virtual QJsonObject toJson() const;
    virtual bool fromJson(const QJsonObject &json);

    QString getTaskType() const;

    void addParameter(const QString &name, QVariant defaultValue = QVariant{});
    QVariant getParameter(const QString &name) const;
    void setParameter(const QString &name, const QVariant &value);

private:
    QString m_taskId;
    QList<TaskPort *> m_inputs;
    QList<TaskPort *> m_outputs;
    QVariantMap m_parameters;
    mutable QMutex m_tasksMutex;
};

} // namespace QodeAssist
