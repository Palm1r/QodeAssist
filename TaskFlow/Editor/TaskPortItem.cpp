// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "TaskPortItem.hpp"

namespace QodeAssist::TaskFlow {

TaskPortItem::TaskPortItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setObjectName("TaskPortItem");
}

TaskPort *TaskPortItem::port() const
{
    return m_port;
}

void TaskPortItem::setPort(TaskPort *newPort)
{
    if (m_port == newPort)
        return;
    m_port = newPort;
    emit portChanged();
}

QString TaskPortItem::name() const
{
    return m_port->name();
}

} // namespace QodeAssist::TaskFlow
