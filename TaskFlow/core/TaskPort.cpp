// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TaskPort.hpp"
#include "TaskConnection.hpp"
#include <QMetaEnum>

namespace QodeAssist::TaskFlow {

TaskPort::TaskPort(const QString &name, ValueType type, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_valueType(type)
{}

QString TaskPort::name() const
{
    return m_name;
}

void TaskPort::setValueType(ValueType type)
{
    if (m_valueType != type)
        m_valueType = type;
}

TaskPort::ValueType TaskPort::valueType() const
{
    return m_valueType;
}

void TaskPort::setValue(const QVariant &value)
{
    if (!isValueTypeCompatible(value)) {
        qWarning() << "TaskPort::setValue - Type mismatch for port" << m_name << "Expected:"
                   << QMetaEnum::fromType<ValueType>().valueToKey(static_cast<int>(m_valueType))
                   << "Got:" << value.typeName();
    }

    if (m_value != value) {
        m_value = value;
        emit valueChanged();
    }
}

QVariant TaskPort::value() const
{
    if (hasConnection() && m_connection->sourcePort()) {
        return m_connection->sourcePort()->m_value;
    }
    return m_value;
}

void TaskPort::setConnection(TaskConnection *connection)
{
    if (m_connection != connection) {
        m_connection = connection;
        emit connectionChanged();
    }
}

TaskConnection *TaskPort::connection() const
{
    return m_connection;
}

bool TaskPort::hasConnection() const
{
    return m_connection != nullptr;
}

bool TaskPort::isValueTypeCompatible(const QVariant &value) const
{
    if (m_valueType == ValueType::Any) {
        return true;
    }

    switch (m_valueType) {
    case ValueType::String:
        return value.canConvert<QString>();

    case ValueType::Number:
        return value.canConvert<double>() || value.canConvert<int>();

    case ValueType::Boolean:
        return value.canConvert<bool>();

    default:
        return false;
    }
}

bool TaskPort::isConnectionTypeCompatible(const TaskPort *sourcePort) const
{
    if (!sourcePort) {
        return false;
    }

    if (sourcePort->valueType() == ValueType::Any || m_valueType == ValueType::Any) {
        return true;
    }

    return sourcePort->valueType() == m_valueType;
}

} // namespace QodeAssist::TaskFlow
