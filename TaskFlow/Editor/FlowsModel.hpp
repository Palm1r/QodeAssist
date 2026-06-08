// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QAbstractListModel>
#include <QObject>

// #include "tasks/Flow.hpp"
#include <FlowManager.hpp>

namespace QodeAssist::TaskFlow {

class FlowsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum FlowRoles { FlowIdRole = Qt::UserRole, FlowDataRole };

    FlowsModel(FlowManager *flowManager, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    void onFlowAdded(const QString &flowId);
    void onFlowRemoved(const QString &flowId);

private:
    FlowManager *m_flowManager;
};

} // namespace QodeAssist::TaskFlow
