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
#include "TaskConnection.hpp"
#include <QString>
#include <QVariant>

namespace QodeAssist {

class TaskPort
{
public:
    explicit TaskPort(const QString &name);

    QString name() const;

    void setConnection(TaskConnection *connection);
    TaskConnection *connection() const;
    bool hasConnection() const;

    void setValue(const QVariant &value);
    QVariant getValue() const;
    QVariant getConnectedValue() const;

private:
    QString m_name;
    TaskConnection *m_connection = nullptr;
    QVariant m_value;
};

} // namespace QodeAssist
