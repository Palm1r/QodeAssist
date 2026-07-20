// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatModel.hpp"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

#include "logger/Logger.hpp"

namespace QodeAssist::Chat {

namespace {

ChatModel::ChatRole toChatRole(Session::RowKind kind)
{
    switch (kind) {
    case Session::RowKind::System:
        return ChatModel::ChatRole::System;
    case Session::RowKind::User:
        return ChatModel::ChatRole::User;
    case Session::RowKind::Assistant:
        return ChatModel::ChatRole::Assistant;
    case Session::RowKind::Tool:
    case Session::RowKind::AgentTool:
        return ChatModel::ChatRole::Tool;
    case Session::RowKind::FileEdit:
        return ChatModel::ChatRole::FileEdit;
    case Session::RowKind::Thinking:
        return ChatModel::ChatRole::Thinking;
    case Session::RowKind::Permission:
        return ChatModel::ChatRole::Permission;
    case Session::RowKind::Plan:
        return ChatModel::ChatRole::Plan;
    }
    return ChatModel::ChatRole::Assistant;
}

bool carriesUsage(const Session::MessageRow &row)
{
    return !row.usage.isEmpty();
}

} // namespace

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    return m_messages.size();
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.size())
        return QVariant();

    const Session::MessageRow &message = m_messages[index.row()];
    switch (static_cast<Roles>(role)) {
    case Roles::RoleType:
        return QVariant::fromValue(toChatRole(message.kind));
    case Roles::Content: {
        return message.content;
    }
    case Roles::Attachments: {
        QVariantList attachmentsList;
        for (const auto &attachment : message.attachments) {
            QVariantMap attachmentMap;
            attachmentMap["fileName"] = attachment.fileName;
            attachmentMap["storedPath"] = attachment.storedPath;

            if (!m_chatFilePath.isEmpty()) {
                QFileInfo fileInfo(m_chatFilePath);
                QString baseName = fileInfo.completeBaseName();
                QString dirPath = fileInfo.absolutePath();
                QString contentFolder = QDir(dirPath).filePath(baseName + "_content");
                QString fullPath = QDir(contentFolder).filePath(attachment.storedPath);
                attachmentMap["filePath"] = fullPath;
            } else {
                attachmentMap["filePath"] = QString();
            }

            attachmentsList.append(attachmentMap);
        }
        return attachmentsList;
    }
    case Roles::IsRedacted: {
        return message.redacted;
    }
    case Roles::PromptTokens:
        return message.usage.promptTokens;
    case Roles::CompletionTokens:
        return message.usage.completionTokens;
    case Roles::CachedPromptTokens:
        return message.usage.cachedPromptTokens;
    case Roles::ReasoningTokens:
        return message.usage.reasoningTokens;
    case Roles::TotalTokens:
        return message.usage.promptTokens + message.usage.completionTokens;
    case Roles::ToolKind:
        return message.toolKind;
    case Roles::ToolStatus:
        return message.toolStatus;
    case Roles::ToolName:
        return message.toolName;
    case Roles::ToolResult:
        return message.toolResult;
    case Roles::ToolDetails:
        return QVariant::fromValue(message.toolDetails);
    case Roles::Images: {
        QVariantList imagesList;
        for (const auto &image : message.images) {
            QVariantMap imageMap;
            imageMap["fileName"] = image.fileName;
            imageMap["storedPath"] = image.storedPath;
            imageMap["mediaType"] = image.mediaType;

            if (!m_chatFilePath.isEmpty()) {
                QFileInfo fileInfo(m_chatFilePath);
                QString baseName = fileInfo.completeBaseName();
                QString dirPath = fileInfo.absolutePath();
                QString contentFolder = QDir(dirPath).filePath(baseName + "_content");
                QString fullPath = QDir(contentFolder).filePath(image.storedPath);
                imageMap["imageUrl"] = QUrl::fromLocalFile(fullPath).toString();
                imageMap["filePath"] = fullPath;
            } else {
                imageMap["imageUrl"] = QString();
                imageMap["filePath"] = QString();
            }

            imagesList.append(imageMap);
        }
        return imagesList;
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
    roles[Roles::IsRedacted] = "isRedacted";
    roles[Roles::Images] = "images";
    roles[Roles::PromptTokens] = "promptTokens";
    roles[Roles::CompletionTokens] = "completionTokens";
    roles[Roles::CachedPromptTokens] = "cachedPromptTokens";
    roles[Roles::ReasoningTokens] = "reasoningTokens";
    roles[Roles::TotalTokens] = "totalTokens";
    roles[Roles::ToolKind] = "toolKind";
    roles[Roles::ToolStatus] = "toolStatus";
    roles[Roles::ToolDetails] = "toolDetails";
    roles[Roles::ToolName] = "toolName";
    roles[Roles::ToolResult] = "toolResult";
    return roles;
}

void ChatModel::resetMessages(const QList<Session::MessageRow> &rows)
{
    beginResetModel();
    m_messages = rows;
    endResetModel();
    emit modelReseted();
    emit sessionUsageChanged();
}

void ChatModel::appendMessages(const QList<Session::MessageRow> &rows)
{
    if (rows.isEmpty())
        return;

    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size() + rows.size() - 1);
    m_messages.append(rows);
    endInsertRows();

    if (std::any_of(rows.cbegin(), rows.cend(), carriesUsage))
        emit sessionUsageChanged();
}

void ChatModel::updateMessage(int index, const Session::MessageRow &row)
{
    if (index < 0 || index >= m_messages.size()) {
        LOG_MESSAGE(QString("Session/model desync: update of row %1 with %2 rows present")
                        .arg(index)
                        .arg(m_messages.size()));
        return;
    }

    const bool usageChanged = m_messages[index].usage != row.usage;
    m_messages[index] = row;
    emit dataChanged(this->index(index), this->index(index));

    if (usageChanged)
        emit sessionUsageChanged();
}

void ChatModel::removeMessages(int first, int count)
{
    if (first < 0 || count <= 0 || first + count > m_messages.size()) {
        LOG_MESSAGE(QString("Session/model desync: removal of %1 rows at %2 with %3 rows present")
                        .arg(count)
                        .arg(first)
                        .arg(m_messages.size()));
        return;
    }

    const bool usageChanged
        = std::any_of(m_messages.cbegin() + first, m_messages.cbegin() + first + count, carriesUsage);

    beginRemoveRows(QModelIndex(), first, first + count - 1);
    m_messages.remove(first, count);
    endRemoveRows();

    if (usageChanged)
        emit sessionUsageChanged();
}

QList<MessagePart> ChatModel::processMessageContent(const QString &content) const
{
    QList<MessagePart> parts;
    static const QRegularExpression codeBlockRegex("```(\\w*)\\n?([\\s\\S]*?)```");
    int lastIndex = 0;
    auto blockMatches = codeBlockRegex.globalMatch(content);

    while (blockMatches.hasNext()) {
        auto match = blockMatches.next();
        if (match.capturedStart() > lastIndex) {
            QString textBetween
                = content.mid(lastIndex, match.capturedStart() - lastIndex).trimmed();
            if (!textBetween.isEmpty()) {
                MessagePart part;
                part.type = MessagePartType::Text;
                part.text = textBetween;
                parts.append(part);
            }
        }

        MessagePart codePart;
        codePart.type = MessagePartType::Code;
        codePart.text = match.captured(2).trimmed();
        codePart.language = match.captured(1);
        parts.append(codePart);

        lastIndex = match.capturedEnd();
    }

    if (lastIndex < content.length()) {
        QString remainingText = content.mid(lastIndex).trimmed();

        static const QRegularExpression unclosedBlockRegex("```(\\w*)\\n?([\\s\\S]*)$");
        auto unclosedMatch = unclosedBlockRegex.match(remainingText);

        if (unclosedMatch.hasMatch()) {
            QString beforeCodeBlock = remainingText.left(unclosedMatch.capturedStart()).trimmed();
            if (!beforeCodeBlock.isEmpty()) {
                MessagePart part;
                part.type = MessagePartType::Text;
                part.text = beforeCodeBlock;
                parts.append(part);
            }

            MessagePart codePart;
            codePart.type = MessagePartType::Code;
            codePart.text = unclosedMatch.captured(2).trimmed();
            codePart.language = unclosedMatch.captured(1);
            parts.append(codePart);
        } else if (!remainingText.isEmpty()) {
            MessagePart part;
            part.type = MessagePartType::Text;
            part.text = remainingText;
            parts.append(part);
        }
    }

    return parts;
}

QVariantList ChatModel::userMessagePreviews(int maxLength) const
{
    QVariantList result;
    const int limit = maxLength > 4 ? maxLength : 80;
    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages[i].kind != Session::RowKind::User)
            continue;
        QString preview = m_messages[i].content;
        preview.replace(QLatin1Char('\n'), QLatin1Char(' '));
        preview.replace(QLatin1Char('\r'), QLatin1Char(' '));
        preview.replace(QLatin1Char('\t'), QLatin1Char(' '));
        preview = preview.simplified();
        if (preview.size() > limit)
            preview = preview.left(limit - 1).trimmed() + QChar(0x2026);
        QVariantMap entry;
        entry[QStringLiteral("messageIndex")] = i;
        entry[QStringLiteral("preview")] = preview;
        result.append(entry);
    }
    return result;
}

int ChatModel::sessionPromptTokens() const
{
    int total = 0;
    for (const auto &m : m_messages)
        total += m.usage.promptTokens;
    return total;
}

int ChatModel::sessionCompletionTokens() const
{
    int total = 0;
    for (const auto &m : m_messages)
        total += m.usage.completionTokens;
    return total;
}

int ChatModel::sessionCachedPromptTokens() const
{
    int total = 0;
    for (const auto &m : m_messages)
        total += m.usage.cachedPromptTokens;
    return total;
}

int ChatModel::sessionTotalTokens() const
{
    return sessionPromptTokens() + sessionCompletionTokens();
}

void ChatModel::setChatFilePath(const QString &filePath)
{
    m_chatFilePath = filePath;
}

QString ChatModel::chatFilePath() const
{
    return m_chatFilePath;
}

} // namespace QodeAssist::Chat
