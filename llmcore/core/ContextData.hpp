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

#pragma once

#include <QString>
#include <QVector>

namespace QodeAssist::LLMCore {

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

} // namespace QodeAssist::LLMCore
