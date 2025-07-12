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

#include "FlowManager.hpp"

#include <Logger.hpp>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

#include "FlowRegistry.hpp"
#include "TaskRegistry.hpp"

namespace QodeAssist::TaskFlow {

FlowManager::FlowManager(QObject *parent)
    : QObject(parent)
    , m_taskRegistry(new TaskRegistry(this))
    , m_flowRegistry(new FlowRegistry(this))
{
    LOG_MESSAGE("FlowManager created");
}

FlowManager::~FlowManager()
{
    clear();
}

// Flow *FlowManager::createFlow(const QString &flowId)
// {
//     Flow *flow = new Flow(flowId, m_taskRegistry, this);
//     if (!m_flows.contains(flow->flowId())) {
//         m_flows.insert(flowId, flow);
//     } else {
//         LOG_MESSAGE(
//             QString("FlowManager::createFlow - flow with id %1 already exists").arg(flow->flowId()));
//     }

//     return flow;
// }

void FlowManager::addFlow(Flow *flow)
{
    qDebug() << "FlowManager::addFlow" << flow->flowId();
    if (!m_flows.contains(flow->flowId())) {
        m_flows.insert(flow->flowId(), flow);
        flow->setParent(this);
        emit flowAdded(flow->flowId());
    } else {
        LOG_MESSAGE(
            QString("FlowManager::addFlow - flow with id %1 already exists").arg(flow->flowId()));
    }
}

void FlowManager::clear()
{
    LOG_MESSAGE(QString("FlowManager::clear - removing %1 flows").arg(m_flows.size()));

    qDeleteAll(m_flows);
    m_flows.clear();
}

QStringList FlowManager::getAvailableTasksTypes()
{
    return m_taskRegistry->getAvailableTypes();
}

QStringList FlowManager::getAvailableFlows()
{
    return m_flowRegistry->getAvailableTypes();
}

QHash<QString, Flow *> FlowManager::flows() const
{
    return m_flows;
}

TaskRegistry *FlowManager::taskRegistry() const
{
    return m_taskRegistry;
}

FlowRegistry *FlowManager::flowRegistry() const
{
    return m_flowRegistry;
}

Flow *FlowManager::getFlow(const QString &flowId) const
{
    // if (flowId.isEmpty()) {
    //     return m_flows.begin().value();
    // }
    // return m_flows.value(flowId, nullptr);
}

} // namespace QodeAssist::TaskFlow
