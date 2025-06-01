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

#include "FlowManager.hpp"

namespace QodeAssist::FlowEditor {

class FlowEditor : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(
        QString currentFlowId READ currentFlowId WRITE setCurrentFlowId NOTIFY currentFlowIdChanged)
    Q_PROPERTY(QStringList availableTaskTypes READ availableTaskTypes CONSTANT)

    QML_ELEMENT
public:
    FlowEditor(QQuickItem *parent = nullptr);

    QString currentFlowId() const;
    void setCurrentFlowId(const QString &newCurrentFlowId);

    QStringList availableTaskTypes() const;

signals:

    void currentFlowIdChanged();

private:
    FlowManager *m_flowManager;
    Flow *m_currentFlow = nullptr;
    QString m_currentFlowId;
};

} // namespace QodeAssist::FlowEditor
