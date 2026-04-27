// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
