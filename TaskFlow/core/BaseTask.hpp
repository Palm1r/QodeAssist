// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
