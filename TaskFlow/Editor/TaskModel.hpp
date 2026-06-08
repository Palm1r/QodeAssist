// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QAbstractListModel>

#include <Flow.hpp>

namespace QodeAssist::TaskFlow {

class TaskModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum TaskRoles { TaskIdRole = Qt::UserRole, TaskDataRole };

    TaskModel(Flow *flow, QObject *parent);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    Flow *m_flow;
};

} // namespace QodeAssist::TaskFlow
