/*
 * Copyright (C) 2024 Petr Mironychev
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

#include <QAbstractListModel>

namespace QodeAssist::Chat {

enum class ChatRole { System, User, Assistant };

struct Message
{
    ChatRole role;
    QString content;
};

class ChatModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles { RoleType = Qt::UserRole, Content };

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addMessage(const QString &content, ChatRole role);
    Q_INVOKABLE void clear();

private:
    QVector<Message> m_messages;
};

} // namespace QodeAssist::Chat

Q_DECLARE_METATYPE(QodeAssist::Chat::ChatRole)
