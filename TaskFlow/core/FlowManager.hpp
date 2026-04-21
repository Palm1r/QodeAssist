// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include "Flow.hpp"

namespace QodeAssist::TaskFlow {

class TaskRegistry;
class FlowRegistry;

class FlowManager : public QObject
{
    Q_OBJECT

public:
    explicit FlowManager(QObject *parent = nullptr);
    ~FlowManager() override;

    // Flow *createFlow(const QString &flowId);
    void addFlow(Flow *flow);

    void clear();

    QStringList getAvailableTasksTypes();
    QStringList getAvailableFlows();

    QHash<QString, Flow *> flows() const;

    TaskRegistry *taskRegistry() const;
    FlowRegistry *flowRegistry() const;

    Flow *getFlow(const QString &flowId = {}) const;

signals:
    void flowAdded(const QString &flowId);
    void flowRemoved(const QString &flowId);

private:
    QHash<QString, Flow *> m_flows;

    TaskRegistry *m_taskRegistry;
    FlowRegistry *m_flowRegistry;
};

} // namespace QodeAssist::TaskFlow
