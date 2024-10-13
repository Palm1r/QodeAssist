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

#include "ChatModel.hpp"
#include <QtCore/qjsonobject.h>
#include <QtQml>
#include <utils/aspects.h>

#include "GeneralSettings.hpp"

namespace QodeAssist::Chat {

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_totalTokens(0)
{
    auto &settings = Settings::generalSettings();

    connect(&settings.chatTokensThreshold,
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
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Roles::RoleType] = "roleType";
    roles[Roles::Content] = "content";
    return roles;
}

QVector<ChatModel::Message> ChatModel::getChatHistory() const
{
    return m_messages;
}

void ChatModel::trim()
{
    while (m_totalTokens > tokensThreshold()) {
        if (!m_messages.isEmpty()) {
            m_totalTokens -= m_messages.first().tokenCount;
            beginRemoveRows(QModelIndex(), 0, 0);
            m_messages.removeFirst();
            endRemoveRows();
        } else {
            break;
        }
    }
}

int ChatModel::estimateTokenCount(const QString &text) const
{
    return text.length() / 4;
}

void ChatModel::addMessage(const QString &content, ChatRole role)
{
    int tokenCount = estimateTokenCount(content);
    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append({role, content, tokenCount});
    m_totalTokens += tokenCount;
    endInsertRows();
    trim();
    emit totalTokensChanged();
}

void ChatModel::clear()
{
    beginResetModel();
    m_messages.clear();
    m_totalTokens = 0;
    endResetModel();
    emit totalTokensChanged();
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
            QString textBetween = content.mid(lastIndex, match.capturedStart() - lastIndex).trimmed();
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

QJsonArray ChatModel::prepareMessagesForRequest(LLMCore::ContextData context) const
{
    QJsonArray messages;

    messages.append(QJsonObject{{"role", "system"}, {"content", context.systemPrompt}});

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
        messages.append(QJsonObject{{"role", role}, {"content", message.content}});
    }

    return messages;
}

int ChatModel::totalTokens() const
{
    return m_totalTokens;
}

int ChatModel::tokensThreshold() const
{
    auto &settings = Settings::generalSettings();
    return settings.chatTokensThreshold();
}

} // namespace QodeAssist::Chat
