// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

#include "session/ConversationHistory.hpp"

namespace QodeAssist::Session {

enum class RowKind {
    User,
    Assistant,
    System,
    Tool,
    AgentTool,
    FileEdit,
    Thinking,
    Permission,
    Plan
};

struct MessageRow
{
    RowKind kind = RowKind::User;
    QString id;
    QString content;
    bool redacted = false;
    QString signature;
    QString toolName;
    QJsonObject toolArguments;
    QString toolResult;
    QString toolKind;
    QString toolStatus;
    QJsonObject toolDetails;
    QList<AttachmentBlock> attachments;
    QList<ImageBlock> images;
    Usage usage;

    bool operator==(const MessageRow &other) const = default;

    friend QDebug operator<<(QDebug debug, const MessageRow &row)
    {
        return debug.nospace() << "Row(kind=" << int(row.kind) << ", id=" << row.id
                               << ", content=" << row.content << ", sig=" << row.signature
                               << ", redacted=" << row.redacted << ", tool=" << row.toolName
                               << ", args=" << row.toolArguments << ", result=" << row.toolResult
                               << ", toolKind=" << row.toolKind << ", toolStatus=" << row.toolStatus
                               << ", toolDetails=" << row.toolDetails
                               << ", attachments=" << row.attachments << ", images=" << row.images
                               << ", " << row.usage << ")";
    }
};

enum class RowAudience { Prompt, Compression, TokenCount };

enum class RowTreatment { Omit, UserText, AssistantText, AssistantThinking, ToolExchange };

bool isTranscriptOnlyRow(RowKind kind);
RowTreatment rowTreatmentFor(RowAudience audience, RowKind kind);

bool isTerminalToolStatus(const QString &status);
QString restoredToolStatus(QString status);

std::optional<MessageRow> projectBlockToRow(const Message &message, const ContentBlock &block);
QList<MessageRow> projectMessageToRows(const Message &message);
QList<MessageRow> projectToRows(const ConversationHistory &history);
ConversationHistory buildFromRows(const QList<MessageRow> &rows);

} // namespace QodeAssist::Session
