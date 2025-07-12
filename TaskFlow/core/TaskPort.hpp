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
