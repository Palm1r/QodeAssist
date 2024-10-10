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
#include <QJsonArray>
#include <qqmlintegration.h>

namespace QodeAssist::Chat {

class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ChatModel no need to create")
public:
    enum Roles { RoleType = Qt::UserRole, Content };

    enum ChatRole { System, User, Assistant };
    Q_ENUM(ChatRole)

    struct Message
    {
        ChatRole role;
        QString content;
        int tokenCount;
    };

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addMessage(const QString &content, ChatRole role);
    Q_INVOKABLE void clear();

    QVector<Message> getChatHistory() const;
    QString getSystemPrompt() const;
    void setSystemPrompt(const QString &prompt);
    QJsonArray prepareMessagesForRequest() const;

private:
    void trim();
    int estimateTokenCount(const QString &text) const;

    QVector<Message> m_messages;
    QString m_systemPrompt;
    int m_totalTokens = 0;
    static const int MAX_HISTORY_SIZE = 50;
    static const int MAX_TOKENS = 4000;
};

} // namespace QodeAssist::Chat
