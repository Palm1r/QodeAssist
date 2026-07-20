// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatHistoryBridge.hpp"

#include "ChatModel.hpp"
#include "session/Session.hpp"

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

ChatModel::Message toChatMessage(const Session::MessageRow &row)
{
    ChatModel::Message message;
    message.role = toChatRole(row.kind);
    message.content = row.content;
    message.id = row.id;
    message.isRedacted = row.redacted;
    message.signature = row.signature;
    message.toolName = row.toolName;
    message.toolArguments = row.toolArguments;
    message.toolResult = row.toolResult;
    message.toolKind = row.toolKind;
    message.toolStatus = row.toolStatus;
    message.toolDetails = row.toolDetails.toVariantMap();

    for (const Session::AttachmentBlock &attachment : row.attachments)
        message.attachments.append(Context::ContentFile{attachment.fileName, attachment.storedPath});

    for (const Session::ImageBlock &image : row.images)
        message.images.append(
            ChatModel::ImageAttachment{image.fileName, image.storedPath, image.mediaType});

    message.promptTokens = row.usage.promptTokens;
    message.completionTokens = row.usage.completionTokens;
    message.cachedPromptTokens = row.usage.cachedPromptTokens;
    message.reasoningTokens = row.usage.reasoningTokens;

    return message;
}

QVector<ChatModel::Message> toChatMessages(const QList<Session::MessageRow> &rows)
{
    QVector<ChatModel::Message> messages;
    messages.reserve(rows.size());
    for (const Session::MessageRow &row : rows)
        messages.append(toChatMessage(row));
    return messages;
}

} // namespace

ChatHistoryBridge::ChatHistoryBridge(Session::Session *session, ChatModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
{
    connect(session, &Session::Session::rowsReset, this, &ChatHistoryBridge::onRowsReset);
    connect(session, &Session::Session::rowsAppended, this, &ChatHistoryBridge::onRowsAppended);
    connect(session, &Session::Session::rowUpdated, this, &ChatHistoryBridge::onRowUpdated);
    connect(session, &Session::Session::rowsRemoved, this, &ChatHistoryBridge::onRowsRemoved);

    onRowsReset(session->rows());
}

void ChatHistoryBridge::onRowsReset(const QList<Session::MessageRow> &rows)
{
    if (m_model)
        m_model->resetMessages(toChatMessages(rows));
}

void ChatHistoryBridge::onRowsAppended(const QList<Session::MessageRow> &rows)
{
    if (m_model)
        m_model->appendMessages(toChatMessages(rows));
}

void ChatHistoryBridge::onRowUpdated(int index, const Session::MessageRow &row)
{
    if (m_model)
        m_model->updateMessage(index, toChatMessage(row));
}

void ChatHistoryBridge::onRowsRemoved(int first, int count)
{
    if (m_model)
        m_model->removeMessages(first, count);
}

} // namespace QodeAssist::Chat
