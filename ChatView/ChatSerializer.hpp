// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "ChatModel.hpp"

namespace QodeAssist::Chat {

struct SerializationResult
{
    bool success{false};
    QString errorMessage;
};

class ChatSerializer
{
public:
    static SerializationResult saveToFile(const ChatModel *model, const QString &filePath);
    static SerializationResult loadFromFile(ChatModel *model, const QString &filePath);

    // Public for testing purposes
    static QJsonObject serializeMessage(const ChatModel::Message &message, const QString &chatFilePath);
    static ChatModel::Message deserializeMessage(const QJsonObject &json, const QString &chatFilePath);
    static QJsonObject serializeChat(const ChatModel *model, const QString &chatFilePath);
    static bool deserializeChat(ChatModel *model, const QJsonObject &json, const QString &chatFilePath);

    // Content management (images and text files)
    static QString getChatContentFolder(const QString &chatFilePath);
    static bool saveContentToStorage(const QString &chatFilePath, 
                                      const QString &fileName,
                                      const QString &base64Data,
                                      QString &storedPath);
    static QString loadContentFromStorage(const QString &chatFilePath, const QString &storedPath);

private:
    static const QString VERSION;
    static constexpr int CURRENT_VERSION = 1;

    static bool ensureDirectoryExists(const QString &filePath);
    static bool validateVersion(const QString &version);
};

} // namespace QodeAssist::Chat
