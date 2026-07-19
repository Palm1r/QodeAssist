// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatHistoryBridge.hpp"

#include "ChatModel.hpp"
#include "session/HistoryProjection.hpp"

namespace QodeAssist::Chat {

namespace {

Session::RowKind toRowKind(ChatModel::ChatRole role)
{
    switch (role) {
    case ChatModel::ChatRole::System:
        return Session::RowKind::System;
    case ChatModel::ChatRole::User:
        return Session::RowKind::User;
    case ChatModel::ChatRole::Assistant:
        return Session::RowKind::Assistant;
    case ChatModel::ChatRole::Tool:
        return Session::RowKind::Tool;
    case ChatModel::ChatRole::FileEdit:
        return Session::RowKind::FileEdit;
    case ChatModel::ChatRole::Thinking:
        return Session::RowKind::Thinking;
    }
    return Session::RowKind::Assistant;
}

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
        return ChatModel::ChatRole::Tool;
    case Session::RowKind::FileEdit:
        return ChatModel::ChatRole::FileEdit;
    case Session::RowKind::Thinking:
        return ChatModel::ChatRole::Thinking;
    }
    return ChatModel::ChatRole::Assistant;
}

Session::MessageRow toRow(const ChatModel::Message &message)
{
    Session::MessageRow row;
    row.kind = toRowKind(message.role);
    row.id = message.id;
    row.content = message.content;
    row.redacted = message.isRedacted;
    row.signature = message.signature;
    row.toolName = message.toolName;
    row.toolArguments = message.toolArguments;
    row.toolResult = message.toolResult;

    for (const Context::ContentFile &attachment : message.attachments)
        row.attachments.append(Session::AttachmentBlock{attachment.filename, attachment.content});

    for (const ChatModel::ImageAttachment &image : message.images)
        row.images.append(Session::ImageBlock{image.fileName, image.storedPath, image.mediaType});

    row.usage = Session::Usage{
        message.promptTokens,
        message.completionTokens,
        message.cachedPromptTokens,
        message.reasoningTokens};

    return row;
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

} // namespace

Session::ConversationHistory ChatHistoryBridge::readHistory(const ChatModel *model)
{
    QList<Session::MessageRow> rows;
    for (const ChatModel::Message &message : model->getChatHistory())
        rows.append(toRow(message));

    return Session::buildFromRows(rows);
}

void ChatHistoryBridge::applyHistory(ChatModel *model, const Session::ConversationHistory &history)
{
    const QList<Session::MessageRow> rows = Session::projectToRows(history);

    model->clear();
    model->setLoadingFromHistory(true);

    for (const Session::MessageRow &row : rows)
        model->appendMessage(toChatMessage(row));

    model->setLoadingFromHistory(false);
}

} // namespace QodeAssist::Chat
