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
#include <QMutex>
#include <QObject>

#include "BaseTask.hpp"
#include "TaskConnection.hpp"
#include "TaskRegistry.hpp"

namespace QodeAssist {

enum class FlowState { Success, Failed, Cancelled };

class Flow : public QObject
{
    Q_OBJECT

public:
    explicit Flow(const QString &flowId, QObject *parent = nullptr);
    ~Flow() override;

    QString flowId() const;

    void addTask(BaseTask *task);

    void addConnection(
        BaseTask *sourceTask, TaskPort *sourcePort, BaseTask *targetTask, TaskPort *targetPort);

    QFuture<FlowState> executeAsync();

    static QString flowStateAsString(FlowState state);

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);
    QByteArray toJsonData() const;
    bool fromJsonData(const QByteArray &jsonData);
    BaseTask *createTaskByType(
        const QString &taskType, const QString &taskId, const QJsonObject &params);
    void registerFlowTasks();

    virtual FlowState execute();

    BaseTask *createTaskFromObject(const QJsonObject &taskObj);
    QStringList getAvailableTaskTypes() const;

private:
    QString m_flowId;
    QHash<QString, BaseTask *> m_tasks;
    QList<TaskConnection *> m_connections;
    TaskRegistry m_registry;
};

} // namespace QodeAssist
