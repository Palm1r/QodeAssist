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

namespace QodeAssist::FlowEditor {

FlowEditor::FlowEditor(QQuickItem *parent)
    : QQuickItem(parent)
    , m_flowManager(new FlowManager(this))
{}

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
    if (m_currentFlow) {
        return m_currentFlow->getAvailableTaskTypes();
    }

    TaskRegistry tempRegistry = TaskRegistry::createWithDefaults();
    return tempRegistry.getAvailableTypes();
}

} // namespace QodeAssist::FlowEditor
