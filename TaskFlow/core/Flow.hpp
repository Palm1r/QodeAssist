// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFuture>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QMutex>
#include <QObject>

#include "BaseTask.hpp"
#include "TaskConnection.hpp"

namespace QodeAssist::TaskFlow {

enum class FlowState { Success, Failed, Cancelled };

class Flow : public QObject
{
    Q_OBJECT

public:
    explicit Flow(QObject *parent = nullptr);
    ~Flow() override;

    QString flowId() const;
    void setFlowId(const QString &flowId);

    void addTask(BaseTask *task);
    void removeTask(const QString &taskId);
    void removeTask(BaseTask *task);

    BaseTask *getTask(const QString &taskId) const;
    bool hasTask(const QString &taskId) const;
    QHash<QString, BaseTask *> tasks() const;

    TaskConnection *addConnection(TaskPort *sourcePort, TaskPort *targetPort);
    void removeConnection(TaskConnection *connection);
    QList<TaskConnection *> connections() const;

    QFuture<FlowState> executeAsync();
    virtual FlowState execute();

    bool isValid() const;
    bool hasCircularDependencies() const;

    static QString flowStateAsString(FlowState state);
    QStringList getTaskIds() const;

signals:
    void taskAdded(const QString &taskId);
    void taskRemoved(const QString &taskId);
    void connectionAdded(QodeAssist::TaskFlow::TaskConnection *connection);
    void connectionRemoved(QodeAssist::TaskFlow::TaskConnection *connection);
    void executionStarted();
    void executionFinished(FlowState result);

private:
    QString m_flowId;
    QHash<QString, BaseTask *> m_tasks;
    QList<TaskConnection *> m_connections;
    mutable QMutex m_flowMutex;

    QList<BaseTask *> getExecutionOrder() const;
    bool detectCircularDependencies() const;
    void visitTask(
        BaseTask *task,
        QSet<BaseTask *> &visited,
        QSet<BaseTask *> &recursionStack,
        bool &hasCycle) const;
    QList<BaseTask *> getTaskDependencies(BaseTask *task) const;
};

} // namespace QodeAssist::TaskFlow

Q_DECLARE_METATYPE(QodeAssist::TaskFlow::Flow *)
Q_DECLARE_METATYPE(QodeAssist::TaskFlow::FlowState)
