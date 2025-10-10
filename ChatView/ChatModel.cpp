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
#include "Logger.hpp"

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

    if (!m_messages.isEmpty() && !id.isEmpty() && m_messages.last().id == id
        && m_messages.last().role == role) {
        Message &lastMessage = m_messages.last();
        lastMessage.content = content;
        lastMessage.attachments = attachments;
        LOG_MESSAGE(QString("Updated message: role=%1, id=%2").arg(role).arg(id));
        emit dataChanged(index(m_messages.size() - 1), index(m_messages.size() - 1));
    } else {
        beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
        Message newMessage{role, content, id};
        newMessage.attachments = attachments;
        m_messages.append(newMessage);
        endInsertRows();
        LOG_MESSAGE(QString("Added new message: role=%1, id=%2, index=%3")
                        .arg(role)
                        .arg(id)
                        .arg(m_messages.size() - 1));
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
    bool foundCodeBlock = blockMatches.hasNext();

    while (blockMatches.hasNext()) {
        auto match = blockMatches.next();
        if (match.capturedStart() > lastIndex) {
            QString textBetween
                = content.mid(lastIndex, match.capturedStart() - lastIndex).trimmed();
            if (!textBetween.isEmpty()) {
                parts.append({MessagePartType::Text, textBetween, ""});
            }
        }
        parts.append({MessagePartType::Code, match.captured(2).trimmed(), match.captured(1)});
        lastIndex = match.capturedEnd();
    }

    if (lastIndex < content.length()) {
        QString remainingText = content.mid(lastIndex).trimmed();

        QRegularExpression unclosedBlockRegex("```(\\w*)\\n?([\\s\\S]*)$");
        auto unclosedMatch = unclosedBlockRegex.match(remainingText);

        if (unclosedMatch.hasMatch()) {
            QString beforeCodeBlock = remainingText.left(unclosedMatch.capturedStart()).trimmed();
            if (!beforeCodeBlock.isEmpty()) {
                parts.append({MessagePartType::Text, beforeCodeBlock, ""});
            }

            parts.append(
                {MessagePartType::Code,
                 unclosedMatch.captured(2).trimmed(),
                 unclosedMatch.captured(1)});
        } else if (!remainingText.isEmpty()) {
            parts.append({MessagePartType::Text, remainingText, ""});
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

void ChatModel::resetModelTo(int index)
{
    if (index < 0 || index >= m_messages.size())
        return;

    if (index < m_messages.size()) {
        beginRemoveRows(QModelIndex(), index, m_messages.size() - 1);
        m_messages.remove(index, m_messages.size() - index);
        endRemoveRows();
    }
}

void ChatModel::addToolExecutionStatus(
    const QString &requestId, const QString &toolId, const QString &toolName)
{
    QString content = toolName;

    LOG_MESSAGE(QString("Adding tool execution status: requestId=%1, toolId=%2, toolName=%3")
                    .arg(requestId, toolId, toolName));

    if (!m_messages.isEmpty() && !toolId.isEmpty() && m_messages.last().id == toolId
        && m_messages.last().role == ChatRole::Tool) {
        Message &lastMessage = m_messages.last();
        lastMessage.content = content;
        LOG_MESSAGE(QString("Updated existing tool message at index %1").arg(m_messages.size() - 1));
        emit dataChanged(index(m_messages.size() - 1), index(m_messages.size() - 1));
    } else {
        beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
        Message newMessage{ChatRole::Tool, content, toolId};
        m_messages.append(newMessage);
        endInsertRows();
        LOG_MESSAGE(QString("Created new tool message at index %1 with toolId=%2")
                        .arg(m_messages.size() - 1)
                        .arg(toolId));
    }
}

void ChatModel::updateToolResult(
    const QString &requestId, const QString &toolId, const QString &toolName, const QString &result)
{
    if (m_messages.isEmpty() || toolId.isEmpty()) {
        LOG_MESSAGE(QString("Cannot update tool result: messages empty=%1, toolId empty=%2")
                        .arg(m_messages.isEmpty())
                        .arg(toolId.isEmpty()));
        return;
    }

    LOG_MESSAGE(
        QString("Updating tool result: requestId=%1, toolId=%2, toolName=%3, result length=%4")
            .arg(requestId, toolId, toolName)
            .arg(result.length()));

    for (int i = m_messages.size() - 1; i >= 0; --i) {
        if (m_messages[i].id == toolId && m_messages[i].role == ChatRole::Tool) {
            m_messages[i].content = toolName + "\n" + result;
            emit dataChanged(index(i), index(i));
            LOG_MESSAGE(QString("Updated tool result at index %1").arg(i));
            return;
        }
    }

    LOG_MESSAGE(QString("WARNING: Tool message with requestId=%1 toolId=%2 not found!")
                    .arg(requestId, toolId));
}

} // namespace QodeAssist::Chat
