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
 * along with QodeAssist. If not, see <https:
 */

#include "FlowEditor.hpp"

namespace QodeAssist::TaskFlow {

FlowEditor::FlowEditor(QQuickItem *parent)
    : QQuickItem(parent)
{}

void FlowEditor::initialize()
{
    emit availableTaskTypesChanged();
    emit availableFlowsChanged();

    m_flowsModel = new FlowsModel(m_flowManager, this);

    emit flowsModelChanged();

    if (m_flowsModel->rowCount() > 0) {
        setCurrentFlowIndex(0);
    }

    // setCurrentFlowId(m_flowManager->flows().begin().value()->flowId());
    m_currentFlow = m_flowManager->getFlow();
    emit currentFlowChanged();
}

QString FlowEditor::currentFlowId() const
{
    return m_currentFlowId;
}

void FlowEditor::setCurrentFlowId(const QString &newCurrentFlowId)
{
    if (m_currentFlowId == newCurrentFlowId)
        return;
    m_currentFlowId = newCurrentFlowId;
    emit currentFlowIdChanged();
}

QStringList FlowEditor::availableTaskTypes() const
{
    if (m_flowManager)
        return m_flowManager->getAvailableTasksTypes();
    else {
        return {"No flow manager"};
    }
}

QStringList FlowEditor::availableFlows() const
{
    if (m_flowManager) {
        auto flows = m_flowManager->getAvailableFlows();
        return flows.size() > 0 ? flows : QStringList{"No flows"};
    } else {
        return {"No flow manager"};
    }
}

void FlowEditor::setFlowManager(FlowManager *newFlowManager)
{
    if (m_flowManager == newFlowManager)
        return;
    m_flowManager = newFlowManager;

    initialize();
}

FlowsModel *FlowEditor::flowsModel() const
{
    return m_flowsModel;
}

int FlowEditor::currentFlowIndex() const
{
    return m_currentFlowIndex;
}

void FlowEditor::setCurrentFlowIndex(int newCurrentFlowIndex)
{
    if (m_currentFlowIndex == newCurrentFlowIndex)
        return;
    m_currentFlowIndex = newCurrentFlowIndex;
    emit currentFlowIndexChanged();
}

Flow *FlowEditor::getFlow(const QString &flowName)
{
    return m_flowManager->getFlow(flowName);
}

Flow *FlowEditor::getCurrentFlow()
{
    return m_flowManager->getFlow(m_currentFlowId);
}

Flow *FlowEditor::currentFlow() const
{
    return m_currentFlow;
}

} // namespace QodeAssist::TaskFlow
