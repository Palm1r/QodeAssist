// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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

    QString text;
    QString thinking;
    QString signature;
    QString toolUseId;
    QString toolName;
    QJsonObject toolInput;
    QString result;
    QString imageData;
    QString mediaType;
    bool isImageUrl = false;

    bool operator==(const ContentBlockEntry &) const = default;
};

struct Message
{
    QString role;
    QVector<ContentBlockEntry> blocks;

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
