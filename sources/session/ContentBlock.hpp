// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QDebug>
#include <QJsonObject>
#include <QString>

#include <variant>

#include "session/AgentPlan.hpp"
#include "session/PermissionRequest.hpp"

namespace QodeAssist::Session {

struct TextBlock
{
    QString text;

    bool operator==(const TextBlock &other) const = default;

    friend QDebug operator<<(QDebug debug, const TextBlock &block)
    {
        return debug.nospace() << "Text(" << block.text << ")";
    }
};

struct ThinkingBlock
{
    QString text;
    QString signature;
    bool redacted = false;

    bool operator==(const ThinkingBlock &other) const = default;

    friend QDebug operator<<(QDebug debug, const ThinkingBlock &block)
    {
        return debug.nospace() << "Thinking(" << block.text << ", sig=" << block.signature
                               << ", redacted=" << block.redacted << ")";
    }
};

struct ToolCallBlock
{
    QString id;
    QString name;
    QJsonObject arguments;
    QString result;
    QString kind;
    QString status;
    QJsonObject details;
    bool fromAgent = false;

    bool operator==(const ToolCallBlock &other) const = default;

    friend QDebug operator<<(QDebug debug, const ToolCallBlock &block)
    {
        return debug.nospace() << "ToolCall(" << block.id << ", " << block.name
                               << ", args=" << block.arguments << ", result=" << block.result
                               << ", kind=" << block.kind << ", status=" << block.status
                               << ", details=" << block.details
                               << ", fromAgent=" << block.fromAgent << ")";
    }
};

struct AttachmentBlock
{
    QString fileName;
    QString storedPath;

    bool operator==(const AttachmentBlock &other) const = default;

    friend QDebug operator<<(QDebug debug, const AttachmentBlock &block)
    {
        return debug.nospace() << "Attachment(" << block.fileName << ", " << block.storedPath
                               << ")";
    }
};

struct ImageBlock
{
    QString fileName;
    QString storedPath;
    QString mediaType;

    bool operator==(const ImageBlock &other) const = default;

    friend QDebug operator<<(QDebug debug, const ImageBlock &block)
    {
        return debug.nospace() << "Image(" << block.fileName << ", " << block.storedPath << ", "
                               << block.mediaType << ")";
    }
};

struct FileEditBlock
{
    QString id;
    QString payload;

    bool operator==(const FileEditBlock &other) const = default;

    friend QDebug operator<<(QDebug debug, const FileEditBlock &block)
    {
        return debug.nospace() << "FileEdit(" << block.id << ", " << block.payload << ")";
    }
};

using ContentBlock = std::variant<
    TextBlock,
    ThinkingBlock,
    ToolCallBlock,
    AttachmentBlock,
    ImageBlock,
    FileEditBlock,
    PermissionBlock,
    PlanBlock>;

inline QDebug operator<<(QDebug debug, const ContentBlock &block)
{
    std::visit([&debug](const auto &alternative) { debug << alternative; }, block);
    return debug;
}

} // namespace QodeAssist::Session
