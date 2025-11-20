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
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtQml>

#include "ChatAssistantSettings.hpp"
#include "Logger.hpp"
#include "context/ChangesManager.h"

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
    
    connect(&Context::ChangesManager::instance(),
            &Context::ChangesManager::fileEditApplied,
            this,
            &ChatModel::onFileEditApplied);
    
    connect(&Context::ChangesManager::instance(),
            &Context::ChangesManager::fileEditRejected,
            this,
            &ChatModel::onFileEditRejected);
    
    connect(&Context::ChangesManager::instance(),
            &Context::ChangesManager::fileEditArchived,
            this,
            &ChatModel::onFileEditArchived);
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
    case Roles::IsRedacted: {
        return message.isRedacted;
    }
    case Roles::Images: {
        QVariantList imagesList;
        for (const auto &image : message.images) {
            QVariantMap imageMap;
            imageMap["fileName"] = image.fileName;
            imageMap["storedPath"] = image.storedPath;
            imageMap["mediaType"] = image.mediaType;
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
    return roles;
}

void ChatModel::addMessage(
    const QString &content,
    ChatRole role,
    const QString &id,
    const QList<Context::ContentFile> &attachments,
    const QList<ImageAttachment> &images)
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
        lastMessage.images = images;
        emit dataChanged(index(m_messages.size() - 1), index(m_messages.size() - 1));
    } else {
        beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
        Message newMessage{role, content, id};
        newMessage.attachments = attachments;
        newMessage.images = images;
        m_messages.append(newMessage);
        endInsertRows();
        
        if (m_loadingFromHistory && role == ChatRole::FileEdit) {
            const QString marker = "QODEASSIST_FILE_EDIT:";
            if (content.contains(marker)) {
                int markerPos = content.indexOf(marker);
                int jsonStart = markerPos + marker.length();
                
                if (jsonStart < content.length()) {
                    QString jsonStr = content.mid(jsonStart);
                    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                    
                    if (doc.isObject()) {
                        QJsonObject editData = doc.object();
                        QString editId = editData.value("edit_id").toString();
                        QString filePath = editData.value("file").toString();
                        QString oldContent = editData.value("old_content").toString();
                        QString newContent = editData.value("new_content").toString();
                        QString originalStatus = editData.value("status").toString();
                        
                        if (!editId.isEmpty() && !filePath.isEmpty()) {
                            Context::ChangesManager::instance().addFileEdit(
                                editId, filePath, oldContent, newContent, false, true);
                            
                            editData["status"] = "archived";
                            editData["status_message"] = "Loaded from chat history";
                            
                            QString updatedContent = marker 
                                + QString::fromUtf8(QJsonDocument(editData).toJson(QJsonDocument::Compact));
                            m_messages.last().content = updatedContent;
                            
                            emit dataChanged(index(m_messages.size() - 1), index(m_messages.size() - 1));
                            
                            LOG_MESSAGE(QString("Registered historical file edit: %1 (original status: %2, now: archived)")
                                            .arg(editId, originalStatus));
                        }
                    }
                }
            }
        }
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

        QRegularExpression unclosedBlockRegex("```(\\w*)\\n?([\\s\\S]*)$");
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
        case ChatRole::Tool:
        case ChatRole::FileEdit:
            continue;
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

    bool toolMessageFound = false;
    for (int i = m_messages.size() - 1; i >= 0; --i) {
        if (m_messages[i].id == toolId && m_messages[i].role == ChatRole::Tool) {
            m_messages[i].content = toolName + "\n" + result;
            emit dataChanged(index(i), index(i));
            toolMessageFound = true;
            LOG_MESSAGE(QString("Updated tool result at index %1").arg(i));
            break;
        }
    }

    if (!toolMessageFound) {
        LOG_MESSAGE(QString("WARNING: Tool message with requestId=%1 toolId=%2 not found!")
                        .arg(requestId, toolId));
    }

    const QString marker = "QODEASSIST_FILE_EDIT:";
    if (result.contains(marker)) {
        LOG_MESSAGE(QString("File edit marker detected in tool result"));

        int markerPos = result.indexOf(marker);
        int jsonStart = markerPos + marker.length();

        if (jsonStart < result.length()) {
            QString jsonStr = result.mid(jsonStart);

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);

            if (parseError.error != QJsonParseError::NoError) {
                LOG_MESSAGE(QString("ERROR: Failed to parse file edit JSON at offset %1: %2")
                                .arg(parseError.offset)
                                .arg(parseError.errorString()));
            } else if (!doc.isObject()) {
                LOG_MESSAGE(
                    QString("ERROR: Parsed JSON is not an object, is array=%1").arg(doc.isArray()));
            } else {
                QJsonObject editData = doc.object();
                
                QString editId = editData.value("edit_id").toString();
                
                if (editId.isEmpty()) {
                    editId = QString("edit_%1").arg(QDateTime::currentMSecsSinceEpoch());
                }

                LOG_MESSAGE(QString("Adding FileEdit message, editId=%1").arg(editId));

                beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
                Message fileEditMsg;
                fileEditMsg.role = ChatRole::FileEdit;
                fileEditMsg.content = result;
                fileEditMsg.id = editId;
                m_messages.append(fileEditMsg);
                endInsertRows();

                LOG_MESSAGE(QString("Added FileEdit message with editId=%1").arg(editId));
            }
        }
    }
}

void ChatModel::addThinkingBlock(
    const QString &requestId, const QString &thinking, const QString &signature)
{
    LOG_MESSAGE(QString("Adding thinking block: requestId=%1, thinking length=%2, signature length=%3")
                    .arg(requestId)
                    .arg(thinking.length())
                    .arg(signature.length()));

    QString displayContent = thinking;
    if (!signature.isEmpty()) {
        displayContent += "\n[Signature: " + signature.left(40) + "...]";
    }

    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    Message thinkingMessage;
    thinkingMessage.role = ChatRole::Thinking;
    thinkingMessage.content = displayContent;
    thinkingMessage.id = requestId;
    thinkingMessage.isRedacted = false;
    thinkingMessage.signature = signature;
    m_messages.append(thinkingMessage);
    endInsertRows();
    LOG_MESSAGE(QString("Added thinking message at index %1 with signature length=%2")
                    .arg(m_messages.size() - 1).arg(signature.length()));
}

void ChatModel::addRedactedThinkingBlock(const QString &requestId, const QString &signature)
{
    LOG_MESSAGE(
        QString("Adding redacted thinking block: requestId=%1, signature length=%2")
            .arg(requestId)
            .arg(signature.length()));

    QString displayContent = "[Thinking content redacted by safety systems]";
    if (!signature.isEmpty()) {
        displayContent += "\n[Signature: " + signature.left(40) + "...]";
    }

    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    Message thinkingMessage;
    thinkingMessage.role = ChatRole::Thinking;
    thinkingMessage.content = displayContent;
    thinkingMessage.id = requestId;
    thinkingMessage.isRedacted = true;
    thinkingMessage.signature = signature;
    m_messages.append(thinkingMessage);
    endInsertRows();
    LOG_MESSAGE(QString("Added redacted thinking message at index %1 with signature length=%2")
                    .arg(m_messages.size() - 1).arg(signature.length()));
}

void ChatModel::updateMessageContent(const QString &messageId, const QString &newContent)
{
    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages[i].id == messageId) {
            m_messages[i].content = newContent;
            emit dataChanged(index(i), index(i));
            LOG_MESSAGE(QString("Updated message content for id: %1").arg(messageId));
            break;
        }
    }
}

void ChatModel::setLoadingFromHistory(bool loading)
{
    m_loadingFromHistory = loading;
    LOG_MESSAGE(QString("ChatModel loading from history: %1").arg(loading ? "true" : "false"));
}

bool ChatModel::isLoadingFromHistory() const
{
    return m_loadingFromHistory;
}

void ChatModel::onFileEditApplied(const QString &editId)
{
    updateFileEditStatus(editId, "applied", "Successfully applied");
}

void ChatModel::onFileEditRejected(const QString &editId)
{
    updateFileEditStatus(editId, "rejected", "Rejected by user");
}

void ChatModel::onFileEditArchived(const QString &editId)
{
    updateFileEditStatus(editId, "archived", "Archived (from previous conversation turn)");
}

void ChatModel::updateFileEditStatus(const QString &editId, const QString &status, const QString &statusMessage)
{
    const QString marker = "QODEASSIST_FILE_EDIT:";
    
    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages[i].role == ChatRole::FileEdit && m_messages[i].id == editId) {
            const QString &content = m_messages[i].content;
            
            if (content.contains(marker)) {
                int markerPos = content.indexOf(marker);
                int jsonStart = markerPos + marker.length();
                
                if (jsonStart < content.length()) {
                    QString jsonStr = content.mid(jsonStart);
                    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                    
                    if (doc.isObject()) {
                        QJsonObject editData = doc.object();
                        
                        editData["status"] = status;
                        editData["status_message"] = statusMessage;
                        
                        QString updatedContent = marker 
                            + QString::fromUtf8(QJsonDocument(editData).toJson(QJsonDocument::Compact));
                        
                        m_messages[i].content = updatedContent;
                        
                        emit dataChanged(index(i), index(i));
                        
                        LOG_MESSAGE(QString("Updated FileEdit message status: editId=%1, status=%2")
                                        .arg(editId, status));
                        break;
                    }
                }
            }
        }
    }
}

} // namespace QodeAssist::Chat
