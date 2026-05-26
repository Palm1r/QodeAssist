// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <optional>

namespace QodeAssist::Templates {

struct ContentBlockEntry
{
    enum class Kind {
        Text,
        Thinking,
        RedactedThinking,
        ToolUse,
        ToolResult,
        Image,
    };

    Kind kind = Kind::Text;

    QString text;          // Text
    QString thinking;      // Thinking
    QString signature;     // Thinking / RedactedThinking
    QString toolUseId;     // ToolUse / ToolResult
    QString toolName;      // ToolUse
    QJsonObject toolInput; // ToolUse
    QString result;        // ToolResult
    QString imageData;     // Image (base64 or url)
    QString mediaType;     // Image
    bool isImageUrl = false;

    bool operator==(const ContentBlockEntry &) const = default;
};

struct Message
{
    QString role;
    QVector<ContentBlockEntry> blocks;

    // Convenience for callers that only need a single text block.
    static Message text(const QString &role, const QString &text)
    {
        Message m;
        m.role = role;
        ContentBlockEntry e;
        e.kind = ContentBlockEntry::Kind::Text;
        e.text = text;
        m.blocks.append(std::move(e));
        return m;
    }

    bool operator==(const Message &) const = default;
};

struct FileMetadata
{
    QString filePath;
    QString content;
    bool operator==(const FileMetadata &) const = default;
};

struct ContextData
{
    std::optional<QString> systemPrompt = std::nullopt;
    std::optional<QString> prefix = std::nullopt;
    std::optional<QString> suffix = std::nullopt;
    std::optional<QString> fileContext = std::nullopt;
    std::optional<QVector<Message>> history = std::nullopt;
    std::optional<QList<FileMetadata>> filesMetadata = std::nullopt;

    bool operator==(const ContextData &) const = default;
};

} // namespace QodeAssist::Templates
