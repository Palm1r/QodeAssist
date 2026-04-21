// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

#include "TaskConnection.hpp"

namespace QodeAssist::TaskFlow {

class TaskPort : public QObject
{
    Q_OBJECT

public:
    enum class ValueType {
        Any,    // QVariant
        String, // QString
        Number, // int/double
        Boolean // bool
    };
    Q_ENUM(ValueType)

    explicit TaskPort(
        const QString &name, ValueType type = ValueType::Any, QObject *parent = nullptr);

    QString name() const;

    ValueType valueType() const;
    void setValueType(ValueType type);

    void setValue(const QVariant &value);
    QVariant value() const;

    void setConnection(TaskConnection *connection);
    TaskConnection *connection() const;
    bool hasConnection() const;

    bool isValueTypeCompatible(const QVariant &value) const;
    bool isConnectionTypeCompatible(const TaskPort *sourcePort) const;

signals:
    void valueChanged();
    void connectionChanged();

private:
    QString m_name;
    ValueType m_valueType;
    QVariant m_value;
    TaskConnection *m_connection = nullptr;
};

} // namespace QodeAssist::TaskFlow

Q_DECLARE_METATYPE(QodeAssist::TaskFlow::TaskPort *)
Q_DECLARE_METATYPE(QodeAssist::TaskFlow::TaskPort::ValueType)
