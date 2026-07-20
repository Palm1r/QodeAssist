// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

#include "acp/AgentBinding.hpp"
#include "session/ConversationHistory.hpp"

namespace QodeAssist::Chat {

struct SerializationResult
{
    bool success{false};
    QString errorMessage;
    QString warningMessage;
};

class ChatSerializer
{
public:
    static SerializationResult saveToFile(
        const Session::ConversationHistory &history,
        const Acp::AgentBinding &binding,
        const QString &filePath);
    static SerializationResult loadFromFile(
        Session::ConversationHistory &history,
        Acp::AgentBinding &binding,
        const QString &filePath);

    // Content management (images and text files)
    static QString getChatContentFolder(const QString &chatFilePath);
    static bool saveContentToStorage(const QString &chatFilePath,
                                      const QString &fileName,
                                      const QString &base64Data,
                                      QString &storedPath);
    static QString loadContentFromStorage(const QString &chatFilePath, const QString &storedPath);
    static QByteArray loadRawContentFromStorage(
        const QString &chatFilePath, const QString &storedPath);

private:
    static bool ensureDirectoryExists(const QString &filePath);
};

} // namespace QodeAssist::Chat
