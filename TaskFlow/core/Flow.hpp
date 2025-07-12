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
