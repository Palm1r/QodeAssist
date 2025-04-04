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

#include "ChatModel.hpp"
#include <utils/aspects.h>
#include <QtCore/qjsonobject.h>
#include <QtQml>

#include "ChatAssistantSettings.hpp"

namespace QodeAssist::Chat {

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent)
{
    auto &settings = Settings::chatAssistantSettings();

    connect(
        &settings.chatTokensThreshold,
        &Utils::BaseAspect::changed,
        this,
        &ChatModel::tokensThresholdChanged);
}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    return m_messages.size();
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.size())
        return QVariant();

    const Message &message = m_messages[index.row()];
    switch (static_cast<Roles>(role)) {
    case Roles::RoleType:
        return QVariant::fromValue(message.role);
    case Roles::Content: {
        return message.content;
    }
    case Roles::Attachments: {
        QStringList filenames;
        for (const auto &attachment : message.attachments) {
            filenames << attachment.filename;
        }
        return filenames;
    }
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Roles::RoleType] = "roleType";
    roles[Roles::Content] = "content";
    roles[Roles::Attachments] = "attachments";
    return roles;
}

void ChatModel::addMessage(
    const QString &content,
    ChatRole role,
    const QString &id,
    const QList<Context::ContentFile> &attachments)
{
    QString fullContent = content;
    if (!attachments.isEmpty()) {
        fullContent += "\n\nAttached files list:";
        for (const auto &attachment : attachments) {
            fullContent += QString("\nname: %1\nfile content:\n%2")
                               .arg(attachment.filename, attachment.content);
        }
    }

    if (!m_messages.isEmpty() && !id.isEmpty() && m_messages.last().id == id) {
        Message &lastMessage = m_messages.last();
        lastMessage.content = content;
        lastMessage.attachments = attachments;
        emit dataChanged(index(m_messages.size() - 1), index(m_messages.size() - 1));
    } else {
        beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
        Message newMessage{role, content, id};
        newMessage.attachments = attachments;
        m_messages.append(newMessage);
        endInsertRows();
    }
}

QVector<ChatModel::Message> ChatModel::getChatHistory() const
{
    return m_messages;
}

void ChatModel::clear()
{
    beginResetModel();
    m_messages.clear();
    endResetModel();
    emit modelReseted();
}

QList<MessagePart> ChatModel::processMessageContent(const QString &content) const
{
    QList<MessagePart> parts;
    QRegularExpression codeBlockRegex("```(\\w*)\\n?([\\s\\S]*?)```");
    int lastIndex = 0;
    auto blockMatches = codeBlockRegex.globalMatch(content);

    while (blockMatches.hasNext()) {
        auto match = blockMatches.next();
        if (match.capturedStart() > lastIndex) {
            QString textBetween
                = content.mid(lastIndex, match.capturedStart() - lastIndex).trimmed();
            if (!textBetween.isEmpty()) {
                parts.append({MessagePart::Text, textBetween, ""});
            }
        }
        parts.append({MessagePart::Code, match.captured(2).trimmed(), match.captured(1)});
        lastIndex = match.capturedEnd();
    }

    if (lastIndex < content.length()) {
        QString remainingText = content.mid(lastIndex).trimmed();
        if (!remainingText.isEmpty()) {
            parts.append({MessagePart::Text, remainingText, ""});
        }
    }

    return parts;
}

QJsonArray ChatModel::prepareMessagesForRequest(const QString &systemPrompt) const
{
    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", systemPrompt}});

    for (const auto &message : m_messages) {
        QString role;
        switch (message.role) {
        case ChatRole::User:
            role = "user";
            break;
        case ChatRole::Assistant:
            role = "assistant";
            break;
        default:
            continue;
        }

        QString content
            = message.attachments.isEmpty()
                  ? message.content
                  : message.content + "\n\nAttached files list:"
                        + std::accumulate(
                            message.attachments.begin(),
                            message.attachments.end(),
                            QString(),
                            [](QString acc, const Context::ContentFile &attachment) {
                                return acc
                                       + QString("\nname: %1\nfile content:\n%2")
                                             .arg(attachment.filename, attachment.content);
                            });

        messages.append(QJsonObject{{"role", role}, {"content", content}});
    }

    return messages;
}

int ChatModel::tokensThreshold() const
{
    auto &settings = Settings::chatAssistantSettings();
    return settings.chatTokensThreshold();
}

QString ChatModel::lastMessageId() const
{
    return !m_messages.isEmpty() ? m_messages.last().id : "";
}

} // namespace QodeAssist::Chat
