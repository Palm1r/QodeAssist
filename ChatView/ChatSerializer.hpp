// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QString>

namespace QodeAssist {
class ConversationHistory;
}

namespace QodeAssist::Chat {

struct SerializationResult
{
    bool success{false};
    QString errorMessage;
};

struct StoredContentEntry
{
    QDateTime modified;
    qint64 size = 0;
    QString base64;
};

using StoredContentCache = QHash<QString, StoredContentEntry>;

class ChatSerializer
{
public:
    static SerializationResult saveToFile(
        const ConversationHistory *history, const QString &filePath);
    static SerializationResult loadFromFile(ConversationHistory *history, const QString &filePath);

    // Content management (images and text files)
    static QString getChatContentFolder(const QString &chatFilePath);
    static bool saveContentToStorage(
        const QString &chatFilePath,
        const QString &fileName,
        const QString &base64Data,
        QString &storedPath);
    static QString loadContentFromStorage(
        const QString &chatFilePath,
        const QString &storedPath,
        StoredContentCache *cache = nullptr);

private:
    static const QString VERSION;

    static QJsonObject serializeChat(const ConversationHistory *history);
    static SerializationResult loadCurrent(ConversationHistory *history, const QJsonObject &root);
    static SerializationResult loadLegacy(ConversationHistory *history, const QJsonObject &root);
    static void registerHistoricalFileEdits(const ConversationHistory *history);

    static bool ensureDirectoryExists(const QString &filePath);
    static bool validateVersion(const QString &version);
};

} // namespace QodeAssist::Chat
