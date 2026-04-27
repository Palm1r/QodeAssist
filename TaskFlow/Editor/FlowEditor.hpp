// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
