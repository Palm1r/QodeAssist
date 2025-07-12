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
