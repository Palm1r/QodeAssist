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

#include "ChatSerializer.hpp"
#include "Logger.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace QodeAssist::Chat {

const QString ChatSerializer::VERSION = "0.1";

SerializationResult ChatSerializer::saveToFile(const ChatModel *model, const QString &filePath)
{
    if (!ensureDirectoryExists(filePath)) {
        return {false, "Failed to create directory structure"};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {false, QString("Failed to open file for writing: %1").arg(filePath)};
    }

    QJsonObject root = serializeChat(model);
    QJsonDocument doc(root);

    if (file.write(doc.toJson(QJsonDocument::Indented)) == -1) {
        return {false, QString("Failed to write to file: %1").arg(file.errorString())};
    }

    return {true, QString()};
}

SerializationResult ChatSerializer::loadFromFile(ChatModel *model, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {false, QString("Failed to open file for reading: %1").arg(filePath)};
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        return {false, QString("JSON parse error: %1").arg(error.errorString())};
    }

    QJsonObject root = doc.object();
    QString version = root["version"].toString();

    if (!validateVersion(version)) {
        return {false, QString("Unsupported version: %1").arg(version)};
    }

    if (!deserializeChat(model, root)) {
        return {false, "Failed to deserialize chat data"};
    }

    return {true, QString()};
}

QJsonObject ChatSerializer::serializeMessage(const ChatModel::Message &message)
{
    QJsonObject messageObj;
    messageObj["role"] = static_cast<int>(message.role);
    messageObj["content"] = message.content;
    messageObj["id"] = message.id;
    messageObj["isRedacted"] = message.isRedacted;
    if (!message.signature.isEmpty()) {
        messageObj["signature"] = message.signature;
    }
    return messageObj;
}

ChatModel::Message ChatSerializer::deserializeMessage(const QJsonObject &json)
{
    ChatModel::Message message;
    message.role = static_cast<ChatModel::ChatRole>(json["role"].toInt());
    message.content = json["content"].toString();
    message.id = json["id"].toString();
    message.isRedacted = json["isRedacted"].toBool(false);
    message.signature = json["signature"].toString();
    return message;
}

QJsonObject ChatSerializer::serializeChat(const ChatModel *model)
{
    QJsonArray messagesArray;
    for (const auto &message : model->getChatHistory()) {
        messagesArray.append(serializeMessage(message));
    }

    QJsonObject root;
    root["version"] = VERSION;
    root["messages"] = messagesArray;

    return root;
}

bool ChatSerializer::deserializeChat(ChatModel *model, const QJsonObject &json)
{
    QJsonArray messagesArray = json["messages"].toArray();
    QVector<ChatModel::Message> messages;
    messages.reserve(messagesArray.size());

    for (const auto &messageValue : messagesArray) {
        messages.append(deserializeMessage(messageValue.toObject()));
    }

    model->clear();
    
    model->setLoadingFromHistory(true);
    
    for (const auto &message : messages) {
        model->addMessage(message.content, message.role, message.id);
    }
    
    model->setLoadingFromHistory(false);

    return true;
}

bool ChatSerializer::ensureDirectoryExists(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    return dir.exists() || dir.mkpath(".");
}

bool ChatSerializer::validateVersion(const QString &version)
{
    return version == VERSION;
}

} // namespace QodeAssist::Chat
