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

#include "TaskPort.hpp"

namespace QodeAssist {

TaskPort::TaskPort(const QString &name)
    : m_name(name)
{}

QString TaskPort::name() const
{
    return m_name;
}

void TaskPort::setConnection(TaskConnection *connection)
{
    m_connection = connection;
}

TaskConnection *TaskPort::connection() const
{
    return m_connection;
}

bool TaskPort::hasConnection() const
{
    return m_connection != nullptr;
}

void TaskPort::setValue(const QVariant &value)
{
    m_value = value;
}

QVariant TaskPort::getValue() const
{
    return m_value;
}

QVariant TaskPort::getConnectedValue() const
{
    if (m_connection && m_connection->sourcePort) {
        return m_connection->sourcePort->getValue();
    }
    return QVariant();
}

} // namespace QodeAssist
