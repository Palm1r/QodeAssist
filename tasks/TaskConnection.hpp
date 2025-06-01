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

#include "TaskPort.hpp"
#include <QString>
#include <QVariantMap>

namespace QodeAssist {

class BaseTask;
class TaskPort;

struct TaskConnection
{
    BaseTask *sourceTask = nullptr;
    BaseTask *targetTask = nullptr;

    TaskPort *sourcePort = nullptr;
    TaskPort *targetPort = nullptr;

    bool operator==(const TaskConnection &other) const;

    QString toString() const;
    static TaskConnection *fromString(
        const QString &connectionStr, const QHash<QString, BaseTask *> &tasks);

    QJsonObject toJson() const;
    static TaskConnection *fromJson(const QJsonObject &json, const QHash<QString, BaseTask *> &tasks);
};

} // namespace QodeAssist
