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

#include "ContextData.hpp"
#include "MessagePart.hpp"

#include <QAbstractListModel>
#include <QJsonArray>
#include <qqmlintegration.h>

namespace QodeAssist::Chat {

class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int totalTokens READ totalTokens NOTIFY totalTokensChanged FINAL)
    Q_PROPERTY(int tokensThreshold READ tokensThreshold NOTIFY tokensThresholdChanged FINAL)
    QML_ELEMENT

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
    Q_INVOKABLE QList<MessagePart> processMessageContent(const QString &content) const;

    QVector<Message> getChatHistory() const;
    QJsonArray prepareMessagesForRequest(LLMCore::ContextData context) const;

    int totalTokens() const;
    int tokensThreshold() const;

    QString currentModel() const;

signals:
    void totalTokensChanged();
    void tokensThresholdChanged();

private:
    void trim();
    int estimateTokenCount(const QString &text) const;

    QVector<Message> m_messages;
    int m_totalTokens = 0;
};

} // namespace QodeAssist::Chat
Q_DECLARE_METATYPE(QodeAssist::Chat::ChatModel::Message)
Q_DECLARE_METATYPE(QodeAssist::Chat::MessagePart)
