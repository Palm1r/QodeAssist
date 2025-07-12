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

#include <QObject>
#include <QString>

namespace QodeAssist::TaskFlow {

class BaseTask;
class TaskPort;

class TaskConnection : public QObject
{
    Q_OBJECT

public:
    // Constructor automatically sets up the connection
    explicit TaskConnection(TaskPort *sourcePort, TaskPort *targetPort, QObject *parent = nullptr);

    // Destructor automatically cleans up the connection
    ~TaskConnection() override;

    // Getters
    BaseTask *sourceTask() const;
    BaseTask *targetTask() const;
    TaskPort *sourcePort() const;
    TaskPort *targetPort() const;

    // Validation
    bool isValid() const;
    bool isTypeCompatible() const;

    // Utility
    QString toString() const;

    // Comparison
    bool operator==(const TaskConnection &other) const;

private:
    TaskPort *m_sourcePort;
    TaskPort *m_targetPort;

    void setupConnection();
    void cleanupConnection();
};

} // namespace QodeAssist::TaskFlow
