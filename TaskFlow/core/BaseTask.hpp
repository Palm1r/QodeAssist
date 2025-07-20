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
#include <QMetaType>
#include <QMutex>
#include <QObject>

namespace QodeAssist::TaskFlow {

class TaskPort;

enum class TaskState { Success, Failed, Cancelled };

class BaseTask : public QObject
{
    Q_OBJECT

public:
    explicit BaseTask(QObject *parent = nullptr);
    virtual ~BaseTask();

    QString taskId() const;
    void setTaskId(const QString &taskId);
    QString taskType() const;

    void addInputPort(const QString &name);
    void addOutputPort(const QString &name);

    TaskPort *inputPort(const QString &name) const;
    TaskPort *outputPort(const QString &name) const;

    QList<TaskPort *> getInputPorts() const;
    QList<TaskPort *> getOutputPorts() const;

    virtual TaskState execute() = 0;

    static QString taskStateAsString(TaskState state);

protected:
    QFuture<TaskState> executeAsync();

private:
    QString m_taskId;
    QList<TaskPort *> m_inputs;
    QList<TaskPort *> m_outputs;
    mutable QMutex m_tasksMutex;
};

} // namespace QodeAssist::TaskFlow
