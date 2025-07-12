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

#pragma once

#include <QQuickItem>

#include "FlowsModel.hpp"
#include <FlowManager.hpp>

namespace QodeAssist::TaskFlow {

class FlowEditor : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(
        QString currentFlowId READ currentFlowId WRITE setCurrentFlowId NOTIFY currentFlowIdChanged)
    Q_PROPERTY(
        QStringList availableTaskTypes READ availableTaskTypes NOTIFY availableTaskTypesChanged)
    Q_PROPERTY(QStringList availableFlows READ availableFlows NOTIFY availableFlowsChanged)
    Q_PROPERTY(FlowsModel *flowsModel READ flowsModel NOTIFY flowsModelChanged)
    Q_PROPERTY(int currentFlowIndex READ currentFlowIndex WRITE setCurrentFlowIndex NOTIFY
                   currentFlowIndexChanged)

    Q_PROPERTY(Flow *currentFlow READ currentFlow NOTIFY currentFlowChanged FINAL)

public:
    FlowEditor(QQuickItem *parent = nullptr);

    void initialize();

    QString currentFlowId() const;
    void setCurrentFlowId(const QString &newCurrentFlowId);

    QStringList availableTaskTypes() const;
    QStringList availableFlows() const;

    void setFlowManager(FlowManager *newFlowManager);

    FlowsModel *flowsModel() const;

    int currentFlowIndex() const;
    void setCurrentFlowIndex(int newCurrentFlowIndex);

    Q_INVOKABLE Flow *getFlow(const QString &flowName);
    Q_INVOKABLE Flow *getCurrentFlow();

    Flow *currentFlow() const;

signals:
    void currentFlowIdChanged();
    void availableTaskTypesChanged();
    void availableFlowsChanged();
    void flowsModelChanged();

    void currentFlowIndexChanged();

    void currentFlowChanged();

private:
    FlowManager *m_flowManager = nullptr;
    QString m_currentFlowId;
    FlowsModel *m_flowsModel;
    int m_currentFlowIndex;
    Flow *m_currentFlow = nullptr;
};

} // namespace QodeAssist::TaskFlow
