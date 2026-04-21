// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QVector>

namespace QodeAssist::PluginLLMCore {

struct ImageAttachment
{
    QString data;        // Base64 encoded data or URL
    QString mediaType;   // e.g., "image/png", "image/jpeg", "image/webp", "image/gif"
    bool isUrl = false;  // true if data is URL, false if base64

    bool operator==(const ImageAttachment &) const = default;
};

struct Message
{
    QString role;
    QString content;
    QString signature;
    bool isThinking = false;
    bool isRedacted = false;
    std::optional<QVector<ImageAttachment>> images;

    // clang-format off
    bool operator==(const Message&) const = default;
    // clang-format on
};

struct FileMetadata
{
    QString filePath;
    QString content;
    bool operator==(const FileMetadata &) const = default;
};

struct ContextData
{
    std::optional<QString> systemPrompt;
    std::optional<QString> prefix;
    std::optional<QString> suffix;
    std::optional<QString> fileContext;
    std::optional<QVector<Message>> history;
    std::optional<QList<FileMetadata>> filesMetadata;

    bool operator==(const ContextData &) const = default;
};

} // namespace QodeAssist::PluginLLMCore
