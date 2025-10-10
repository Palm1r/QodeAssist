/*
 * Copyright (C) 2024-2025 Petr Mironychev
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
#include <QtQmlIntegration>

#include "context/ContentFile.hpp"

namespace QodeAssist::Chat {

class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int tokensThreshold READ tokensThreshold NOTIFY tokensThresholdChanged FINAL)
    QML_ELEMENT

public:
    enum ChatRole { System, User, Assistant, Tool };
    Q_ENUM(ChatRole)

    enum Roles { RoleType = Qt::UserRole, Content, Attachments };
    Q_ENUM(Roles)

    struct Message
    {
        ChatRole role;
        QString content;
        QString id;

        QList<Context::ContentFile> attachments;
    };

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addMessage(
        const QString &content,
        ChatRole role,
        const QString &id,
        const QList<Context::ContentFile> &attachments = {});
    Q_INVOKABLE void clear();
    Q_INVOKABLE QList<MessagePart> processMessageContent(const QString &content) const;

    QVector<Message> getChatHistory() const;
    QJsonArray prepareMessagesForRequest(const QString &systemPrompt) const;

    int tokensThreshold() const;

    QString currentModel() const;
    QString lastMessageId() const;

    Q_INVOKABLE void resetModelTo(int index);

    void addToolExecutionStatus(
        const QString &requestId, const QString &toolId, const QString &toolName);
    void updateToolResult(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QString &result);
signals:
    void tokensThresholdChanged();
    void modelReseted();

private:
    QVector<Message> m_messages;
};

} // namespace QodeAssist::Chat
Q_DECLARE_METATYPE(QodeAssist::Chat::ChatModel::Message)
Q_DECLARE_METATYPE(QodeAssist::Chat::MessagePart)
