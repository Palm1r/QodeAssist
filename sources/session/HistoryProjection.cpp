// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/HistoryProjection.hpp"

#include <optional>

namespace QodeAssist::Session {

namespace {

QString signatureSuffix(const QString &signature)
{
    return QString("\n[Signature: %1...]").arg(signature.left(40));
}

QString thinkingDisplayText(const ThinkingBlock &block)
{
    return block.signature.isEmpty() ? block.text : block.text + signatureSuffix(block.signature);
}

QString thinkingTextOf(const MessageRow &row)
{
    QString text = row.content;
    if (row.signature.isEmpty())
        return text;

    const QString suffix = signatureSuffix(row.signature);
    if (text.endsWith(suffix))
        text.chop(suffix.size());
    return text;
}

QString toolDisplayText(const ToolCallBlock &block)
{
    if (block.name.isEmpty())
        return block.result;
    return block.result.isEmpty() ? block.name : block.name + "\n" + block.result;
}

ToolCallBlock toolCallOf(const MessageRow &row)
{
    ToolCallBlock block{row.id, row.toolName, row.toolArguments, row.toolResult};
    if (block.name.isEmpty() && block.result.isEmpty())
        block.result = row.content;
    return block;
}

RowKind textRowKind(MessageRole role)
{
    switch (role) {
    case MessageRole::System:
        return RowKind::System;
    case MessageRole::User:
        return RowKind::User;
    case MessageRole::Assistant:
        return RowKind::Assistant;
    }
    return RowKind::Assistant;
}

} // namespace

QList<MessageRow> projectToRows(const ConversationHistory &history)
{
    QList<MessageRow> rows;

    for (const Message &message : history.messages()) {
        const RowKind textKind = textRowKind(message.role);
        bool usageAssigned = message.usage.isEmpty();
        qsizetype textRowIndex = -1;

        auto appendRow = [&](MessageRow row) {
            if (!usageAssigned && row.id == message.id) {
                row.usage = message.usage;
                usageAssigned = true;
            }
            rows.append(row);
            return rows.size() - 1;
        };

        auto ensureTextRow = [&] {
            if (textRowIndex < 0) {
                MessageRow row;
                row.kind = textKind;
                row.id = message.id;
                textRowIndex = appendRow(row);
            }
            return textRowIndex;
        };

        for (const ContentBlock &block : message.blocks) {
            if (const auto *text = std::get_if<TextBlock>(&block)) {
                MessageRow row;
                row.kind = textKind;
                row.id = message.id;
                row.content = text->text;
                textRowIndex = appendRow(row);
            } else if (const auto *thinking = std::get_if<ThinkingBlock>(&block)) {
                MessageRow row;
                row.kind = RowKind::Thinking;
                row.id = message.id;
                row.content = thinkingDisplayText(*thinking);
                row.redacted = thinking->redacted;
                row.signature = thinking->signature;
                appendRow(row);
            } else if (const auto *tool = std::get_if<ToolCallBlock>(&block)) {
                MessageRow row;
                row.kind = RowKind::Tool;
                row.id = tool->id;
                row.content = toolDisplayText(*tool);
                row.toolName = tool->name;
                row.toolArguments = tool->arguments;
                row.toolResult = tool->result;
                appendRow(row);
            } else if (const auto *edit = std::get_if<FileEditBlock>(&block)) {
                MessageRow row;
                row.kind = RowKind::FileEdit;
                row.id = edit->id;
                row.content = edit->payload;
                appendRow(row);
            } else if (const auto *attachment = std::get_if<AttachmentBlock>(&block)) {
                rows[ensureTextRow()].attachments.append(*attachment);
            } else if (const auto *image = std::get_if<ImageBlock>(&block)) {
                rows[ensureTextRow()].images.append(*image);
            }
        }
    }

    return rows;
}

ConversationHistory buildFromRows(const QList<MessageRow> &rows)
{
    ConversationHistory history;
    std::optional<Message> assistant;

    auto flush = [&] {
        if (assistant) {
            history.append(*assistant);
            assistant.reset();
        }
    };

    auto openAssistant = [&]() -> Message & {
        if (!assistant) {
            Message message;
            message.role = MessageRole::Assistant;
            assistant = message;
        }
        return *assistant;
    };

    for (const MessageRow &row : rows) {
        switch (row.kind) {
        case RowKind::User:
        case RowKind::System: {
            flush();
            Message message;
            message.role = row.kind == RowKind::User ? MessageRole::User : MessageRole::System;
            message.id = row.id;
            message.usage = row.usage;
            message.blocks.append(TextBlock{row.content});
            for (const AttachmentBlock &attachment : row.attachments)
                message.blocks.append(attachment);
            for (const ImageBlock &image : row.images)
                message.blocks.append(image);
            history.append(message);
            break;
        }
        case RowKind::Assistant:
        case RowKind::Thinking: {
            if (assistant && !row.id.isEmpty() && !assistant->id.isEmpty()
                && assistant->id != row.id) {
                flush();
            }
            Message &message = openAssistant();
            if (message.id.isEmpty())
                message.id = row.id;
            if (row.kind == RowKind::Assistant) {
                message.blocks.append(TextBlock{row.content});
            } else {
                message.blocks.append(
                    ThinkingBlock{thinkingTextOf(row), row.signature, row.redacted});
            }
            if (!row.usage.isEmpty())
                message.usage = row.usage;
            break;
        }
        case RowKind::Tool: {
            Message &message = openAssistant();
            message.blocks.append(toolCallOf(row));
            break;
        }
        case RowKind::FileEdit: {
            Message &message = openAssistant();
            message.blocks.append(FileEditBlock{row.id, row.content});
            break;
        }
        }
    }

    flush();

    return history;
}

} // namespace QodeAssist::Session
