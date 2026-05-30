// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatModel.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>

#include <LLMQore/ContentBlocks.hpp>

#include <ConversationHistory.hpp>
#include <Message.hpp>
#include <PluginBlocks.hpp>

#include "context/ChangesManager.h"

namespace QodeAssist::Chat {

namespace {

const QString kFileEditMarker = QStringLiteral("QODEASSIST_FILE_EDIT:");

QString changesStatusToString(Context::ChangesManager::FileEditStatus status)
{
    switch (status) {
    case Context::ChangesManager::Pending: return QStringLiteral("pending");
    case Context::ChangesManager::Applied: return QStringLiteral("applied");
    case Context::ChangesManager::Rejected: return QStringLiteral("rejected");
    case Context::ChangesManager::Archived: return QStringLiteral("archived");
    }
    return QStringLiteral("pending");
}

QString parseEditId(const QString &markerContent)
{
    const int pos = markerContent.indexOf(kFileEditMarker);
    if (pos < 0)
        return {};
    const QString jsonStr = markerContent.mid(pos + kFileEditMarker.length());
    const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject())
        return {};
    return doc.object().value(QStringLiteral("edit_id")).toString();
}

QString collectText(const Message &m)
{
    QString text;
    for (const auto &block : m.blocks()) {
        if (auto *t = dynamic_cast<LLMQore::TextContent *>(block.get())) {
            if (!text.isEmpty())
                text += QLatin1Char('\n');
            text += t->text();
        }
    }
    return text;
}

bool messageIsToolResultsOnly(const Message &m)
{
    bool hasToolResult = false;
    for (const auto &block : m.blocks()) {
        if (dynamic_cast<LLMQore::ToolResultContent *>(block.get()))
            hasToolResult = true;
        else
            return false;
    }
    return hasToolResult;
}

} // namespace

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent)
{
    auto &changes = Context::ChangesManager::instance();
    connect(
        &changes, &Context::ChangesManager::fileEditApplied,
        this, &ChatModel::onFileEditStatusChanged);
    connect(
        &changes, &Context::ChangesManager::fileEditRejected,
        this, &ChatModel::onFileEditStatusChanged);
    connect(
        &changes, &Context::ChangesManager::fileEditUndone,
        this, &ChatModel::onFileEditStatusChanged);
    connect(
        &changes, &Context::ChangesManager::fileEditArchived,
        this, &ChatModel::onFileEditStatusChanged);
}

void ChatModel::setHistory(ConversationHistory *history)
{
    if (m_history == history)
        return;

    if (m_history)
        m_history->disconnect(this);

    m_history = history;

    if (m_history) {
        connect(
            m_history, &ConversationHistory::messageAdded,
            this, &ChatModel::onHistoryMessageAdded);
        connect(
            m_history, &ConversationHistory::messageUpdated,
            this, &ChatModel::onHistoryMessageUpdated);
        connect(m_history, &ConversationHistory::cleared, this, &ChatModel::onHistoryCleared);
        connect(m_history, &ConversationHistory::reset, this, &ChatModel::onHistoryReset);
    }

    beginResetModel();
    rebuildAll();
    endResetModel();
    emit sessionUsageChanged();
}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return QVariant();

    const Row &row = m_rows[index.row()];
    switch (static_cast<Roles>(role)) {
    case Roles::RoleType:
        return QVariant::fromValue(row.kind);
    case Roles::Content:
        if (row.kind == ChatRole::FileEdit)
            return overlayFileEditStatus(row.content, row.editId);
        return row.content;
    case Roles::Attachments:
        return buildAttachmentList(row.attachments);
    case Roles::Images:
        return buildImageList(row.images);
    case Roles::IsRedacted:
        return row.isRedacted;
    case Roles::PromptTokens:
        return m_usageByMessageId.value(row.messageId).prompt;
    case Roles::CompletionTokens:
        return m_usageByMessageId.value(row.messageId).completion;
    case Roles::CachedPromptTokens:
        return m_usageByMessageId.value(row.messageId).cached;
    case Roles::ReasoningTokens:
        return m_usageByMessageId.value(row.messageId).reasoning;
    case Roles::TotalTokens: {
        const Usage u = m_usageByMessageId.value(row.messageId);
        return u.prompt + u.completion;
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
    return roles;
}

QVariantList ChatModel::buildAttachmentList(const QVector<AttachmentRef> &attachments) const
{
    QVariantList list;
    for (const auto &attachment : attachments) {
        QVariantMap map;
        map["fileName"] = attachment.fileName;
        map["storedPath"] = attachment.storedPath;
        if (!m_chatFilePath.isEmpty()) {
            QFileInfo fileInfo(m_chatFilePath);
            const QString contentFolder
                = QDir(fileInfo.absolutePath()).filePath(fileInfo.completeBaseName() + "_content");
            map["filePath"] = QDir(contentFolder).filePath(attachment.storedPath);
        } else {
            map["filePath"] = QString();
        }
        list.append(map);
    }
    return list;
}

QVariantList ChatModel::buildImageList(const QVector<ImageRef> &images) const
{
    QVariantList list;
    for (const auto &image : images) {
        QVariantMap map;
        map["fileName"] = image.fileName;
        map["storedPath"] = image.storedPath;
        map["mediaType"] = image.mediaType;
        if (!m_chatFilePath.isEmpty()) {
            QFileInfo fileInfo(m_chatFilePath);
            const QString contentFolder
                = QDir(fileInfo.absolutePath()).filePath(fileInfo.completeBaseName() + "_content");
            const QString fullPath = QDir(contentFolder).filePath(image.storedPath);
            map["imageUrl"] = QUrl::fromLocalFile(fullPath).toString();
            map["filePath"] = fullPath;
        } else {
            map["imageUrl"] = QString();
            map["filePath"] = QString();
        }
        list.append(map);
    }
    return list;
}

QString ChatModel::overlayFileEditStatus(const QString &content, const QString &editId) const
{
    const int pos = content.indexOf(kFileEditMarker);
    if (pos < 0)
        return content;

    const QString jsonStr = content.mid(pos + kFileEditMarker.length());
    const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject())
        return content;

    QJsonObject obj = doc.object();
    if (!editId.isEmpty()) {
        const auto edit = Context::ChangesManager::instance().getFileEdit(editId);
        if (!edit.editId.isEmpty()) {
            obj["status"] = changesStatusToString(edit.status);
            if (!edit.statusMessage.isEmpty())
                obj["status_message"] = edit.statusMessage;
        }
    }
    return kFileEditMarker
           + QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QHash<QString, QString> ChatModel::buildToolResultMap() const
{
    QHash<QString, QString> results;
    if (!m_history)
        return results;
    for (const auto &m : m_history->messages()) {
        for (const auto &block : m.blocks()) {
            if (auto *tr = dynamic_cast<LLMQore::ToolResultContent *>(block.get()))
                results.insert(tr->toolUseId(), tr->result());
        }
    }
    return results;
}

void ChatModel::appendRowsForMessage(
    int messageIndex, const QHash<QString, QString> &toolResults, QVector<Row> &out) const
{
    if (!m_history || messageIndex < 0 || messageIndex >= m_history->size())
        return;

    const Message &m = m_history->messages()[static_cast<size_t>(messageIndex)];
    const QString id = m.id();

    switch (m.role()) {
    case Message::Role::System: {
        const QString text = collectText(m);
        if (!text.trimmed().isEmpty()) {
            Row row;
            row.kind = ChatRole::System;
            row.messageIndex = messageIndex;
            row.messageId = id;
            row.content = text;
            out.append(std::move(row));
        }
        break;
    }
    case Message::Role::User: {
        QString text;
        QVector<AttachmentRef> attachments;
        QVector<ImageRef> images;
        bool hasDisplayable = false;
        for (const auto &block : m.blocks()) {
            if (auto *t = dynamic_cast<LLMQore::TextContent *>(block.get())) {
                if (!text.isEmpty())
                    text += QLatin1Char('\n');
                text += t->text();
                hasDisplayable = true;
            } else if (auto *sa = dynamic_cast<StoredAttachmentContent *>(block.get())) {
                attachments.append({sa->fileName(), sa->storedPath()});
                hasDisplayable = true;
            } else if (auto *si = dynamic_cast<StoredImageContent *>(block.get())) {
                images.append({si->fileName(), si->storedPath(), si->mediaType()});
                hasDisplayable = true;
            }
        }
        if (hasDisplayable) {
            Row row;
            row.kind = ChatRole::User;
            row.messageIndex = messageIndex;
            row.messageId = id;
            row.content = text;
            row.attachments = std::move(attachments);
            row.images = std::move(images);
            out.append(std::move(row));
        }
        break;
    }
    case Message::Role::Assistant: {
        for (const auto &block : m.blocks()) {
            if (auto *th = dynamic_cast<LLMQore::ThinkingContent *>(block.get())) {
                QString content = th->thinking();
                if (!th->signature().isEmpty())
                    content += QStringLiteral("\n[Signature: ") + th->signature().left(40)
                               + QStringLiteral("...]");
                Row row;
                row.kind = ChatRole::Thinking;
                row.messageIndex = messageIndex;
                row.messageId = id;
                row.content = content;
                out.append(std::move(row));
            } else if (
                auto *rth = dynamic_cast<LLMQore::RedactedThinkingContent *>(block.get())) {
                QString content = QStringLiteral("[Thinking content redacted by safety systems]");
                if (!rth->signature().isEmpty())
                    content += QStringLiteral("\n[Signature: ") + rth->signature().left(40)
                               + QStringLiteral("...]");
                Row row;
                row.kind = ChatRole::Thinking;
                row.messageIndex = messageIndex;
                row.messageId = id;
                row.content = content;
                row.isRedacted = true;
                out.append(std::move(row));
            } else if (auto *t = dynamic_cast<LLMQore::TextContent *>(block.get())) {
                if (!t->text().trimmed().isEmpty()) {
                    Row row;
                    row.kind = ChatRole::Assistant;
                    row.messageIndex = messageIndex;
                    row.messageId = id;
                    row.content = t->text();
                    out.append(std::move(row));
                }
            } else if (auto *tu = dynamic_cast<LLMQore::ToolUseContent *>(block.get())) {
                const QString result = toolResults.value(tu->id());
                Row toolRow;
                toolRow.kind = ChatRole::Tool;
                toolRow.messageIndex = messageIndex;
                toolRow.messageId = id;
                toolRow.content = tu->name() + QLatin1Char('\n') + result;
                out.append(std::move(toolRow));

                if (result.contains(kFileEditMarker)) {
                    Row editRow;
                    editRow.kind = ChatRole::FileEdit;
                    editRow.messageIndex = messageIndex;
                    editRow.messageId = id;
                    editRow.content = result;
                    editRow.editId = parseEditId(result);
                    out.append(std::move(editRow));
                }
            }
        }
        break;
    }
    }
}

void ChatModel::rebuildAll()
{
    m_rows.clear();
    if (!m_history)
        return;
    const QHash<QString, QString> toolResults = buildToolResultMap();
    for (int mi = 0; mi < m_history->size(); ++mi)
        appendRowsForMessage(mi, toolResults, m_rows);
}

int ChatModel::firstRowForMessage(int messageIndex) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].messageIndex >= messageIndex)
            return i;
    }
    return m_rows.size();
}

int ChatModel::startMessageIndexFor(int messageIndex) const
{
    if (!m_history || messageIndex < 0 || messageIndex >= m_history->size())
        return messageIndex;

    const auto &msgs = m_history->messages();
    const Message &m = msgs[static_cast<size_t>(messageIndex)];
    if (m.role() == Message::Role::User && messageIsToolResultsOnly(m)) {
        for (int j = messageIndex - 1; j >= 0; --j) {
            if (msgs[static_cast<size_t>(j)].role() == Message::Role::Assistant)
                return j;
        }
    }
    return messageIndex;
}

void ChatModel::reprojectTail(int startMessageIndex)
{
    if (!m_history)
        return;

    const int oldStart = firstRowForMessage(startMessageIndex);
    const QHash<QString, QString> toolResults = buildToolResultMap();

    QVector<Row> newTail;
    for (int mi = startMessageIndex; mi < m_history->size(); ++mi)
        appendRowsForMessage(mi, toolResults, newTail);

    const int oldCount = m_rows.size() - oldStart;
    const int newCount = newTail.size();
    const int common = qMin(oldCount, newCount);

    for (int i = 0; i < common; ++i)
        m_rows[oldStart + i] = newTail[i];
    if (common > 0)
        emit dataChanged(index(oldStart), index(oldStart + common - 1));

    if (newCount > oldCount) {
        beginInsertRows(QModelIndex(), oldStart + oldCount, oldStart + newCount - 1);
        for (int i = oldCount; i < newCount; ++i)
            m_rows.append(newTail[i]);
        endInsertRows();
    } else if (newCount < oldCount) {
        beginRemoveRows(QModelIndex(), oldStart + newCount, oldStart + oldCount - 1);
        m_rows.remove(oldStart + newCount, oldCount - newCount);
        endRemoveRows();
    }
}

void ChatModel::onHistoryMessageAdded(int index)
{
    reprojectTail(startMessageIndexFor(index));
}

void ChatModel::onHistoryMessageUpdated(int index)
{
    reprojectTail(startMessageIndexFor(index));
}

void ChatModel::onHistoryCleared()
{
    beginResetModel();
    m_rows.clear();
    m_usageByMessageId.clear();
    endResetModel();
    emit modelReseted();
    emit sessionUsageChanged();
}

void ChatModel::onHistoryReset()
{
    beginResetModel();
    rebuildAll();
    endResetModel();
    emit sessionUsageChanged();
}

void ChatModel::onFileEditStatusChanged(const QString &editId)
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].kind == ChatRole::FileEdit && m_rows[i].editId == editId)
            emit dataChanged(index(i), index(i), {Roles::Content});
    }
}

void ChatModel::clear()
{
    if (m_history)
        m_history->clear();
    else
        onHistoryCleared();
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

void ChatModel::resetModelTo(int index)
{
    if (!m_history || index < 0 || index >= m_rows.size())
        return;
    m_history->resetTo(m_rows[index].messageIndex);
}

QVariantList ChatModel::userMessagePreviews(int maxLength) const
{
    QVariantList result;
    const int limit = maxLength > 4 ? maxLength : 80;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].kind != ChatRole::User)
            continue;
        QString preview = m_rows[i].content;
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

void ChatModel::setMessageUsage(
    const QString &messageId,
    int promptTokens,
    int completionTokens,
    int cachedPromptTokens,
    int reasoningTokens)
{
    if (messageId.isEmpty())
        return;

    m_usageByMessageId[messageId]
        = Usage{promptTokens, completionTokens, cachedPromptTokens, reasoningTokens};

    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].messageId == messageId) {
            emit dataChanged(
                index(i),
                index(i),
                {Roles::PromptTokens,
                 Roles::CompletionTokens,
                 Roles::CachedPromptTokens,
                 Roles::ReasoningTokens,
                 Roles::TotalTokens});
        }
    }
    emit sessionUsageChanged();
}

int ChatModel::sessionPromptTokens() const
{
    int total = 0;
    if (m_history) {
        for (const auto &m : m_history->messages())
            total += m_usageByMessageId.value(m.id()).prompt;
    }
    return total;
}

int ChatModel::sessionCompletionTokens() const
{
    int total = 0;
    if (m_history) {
        for (const auto &m : m_history->messages())
            total += m_usageByMessageId.value(m.id()).completion;
    }
    return total;
}

int ChatModel::sessionCachedPromptTokens() const
{
    int total = 0;
    if (m_history) {
        for (const auto &m : m_history->messages())
            total += m_usageByMessageId.value(m.id()).cached;
    }
    return total;
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
