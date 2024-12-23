/* 
 * Copyright (C) 2024 Petr Mironychev
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
    static QJsonObject serializeMessage(const ChatModel::Message &message);
    static ChatModel::Message deserializeMessage(const QJsonObject &json);
    static QJsonObject serializeChat(const ChatModel *model);
    static bool deserializeChat(ChatModel *model, const QJsonObject &json);

private:
    static const QString VERSION;
    static constexpr int CURRENT_VERSION = 1;

    static bool ensureDirectoryExists(const QString &filePath);
    static bool validateVersion(const QString &version);
};

} // namespace QodeAssist::Chat
