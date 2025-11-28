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

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

namespace QodeAssist::Chat {

const QString ChatSerializer::VERSION = "0.1";

SerializationResult ChatSerializer::saveToFile(const ChatModel *model, const QString &filePath)
{
    if (!ensureDirectoryExists(filePath)) {
        return {false, "Failed to create directory structure"};
    }

    QString imagesFolder = getChatImagesFolder(filePath);
    QDir dir;
    if (!dir.exists(imagesFolder)) {
        if (!dir.mkpath(imagesFolder)) {
            LOG_MESSAGE(QString("Warning: Failed to create images folder: %1").arg(imagesFolder));
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {false, QString("Failed to open file for writing: %1").arg(filePath)};
    }

    QJsonObject root = serializeChat(model, filePath);
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

    if (!deserializeChat(model, root, filePath)) {
        return {false, "Failed to deserialize chat data"};
    }

    return {true, QString()};
}

QJsonObject ChatSerializer::serializeMessage(const ChatModel::Message &message, const QString &chatFilePath)
{
    QJsonObject messageObj;
    messageObj["role"] = static_cast<int>(message.role);
    messageObj["content"] = message.content;
    messageObj["id"] = message.id;
    
    if (message.isRedacted) {
        messageObj["isRedacted"] = true;
    }
    
    if (!message.signature.isEmpty()) {
        messageObj["signature"] = message.signature;
    }
    
    if (!message.images.isEmpty()) {
        QJsonArray imagesArray;
        for (const auto &image : message.images) {
            QJsonObject imageObj;
            imageObj["fileName"] = image.fileName;
            imageObj["storedPath"] = image.storedPath;
            imageObj["mediaType"] = image.mediaType;
            imagesArray.append(imageObj);
        }
        messageObj["images"] = imagesArray;
    }
    
    return messageObj;
}

ChatModel::Message ChatSerializer::deserializeMessage(const QJsonObject &json, const QString &chatFilePath)
{
    ChatModel::Message message;
    message.role = static_cast<ChatModel::ChatRole>(json["role"].toInt());
    message.content = json["content"].toString();
    message.id = json["id"].toString();
    message.isRedacted = json["isRedacted"].toBool(false);
    message.signature = json["signature"].toString();
    
    if (json.contains("images")) {
        QJsonArray imagesArray = json["images"].toArray();
        for (const auto &imageValue : imagesArray) {
            QJsonObject imageObj = imageValue.toObject();
            ChatModel::ImageAttachment image;
            image.fileName = imageObj["fileName"].toString();
            image.storedPath = imageObj["storedPath"].toString();
            image.mediaType = imageObj["mediaType"].toString();
            message.images.append(image);
        }
    }
    
    return message;
}

QJsonObject ChatSerializer::serializeChat(const ChatModel *model, const QString &chatFilePath)
{
    QJsonArray messagesArray;
    for (const auto &message : model->getChatHistory()) {
        messagesArray.append(serializeMessage(message, chatFilePath));
    }

    QJsonObject root;
    root["version"] = VERSION;
    root["messages"] = messagesArray;

    return root;
}

bool ChatSerializer::deserializeChat(ChatModel *model, const QJsonObject &json, const QString &chatFilePath)
{
    QJsonArray messagesArray = json["messages"].toArray();
    QVector<ChatModel::Message> messages;
    messages.reserve(messagesArray.size());

    for (const auto &messageValue : messagesArray) {
        messages.append(deserializeMessage(messageValue.toObject(), chatFilePath));
    }

    model->clear();
    
    model->setLoadingFromHistory(true);
    
    for (const auto &message : messages) {
        model->addMessage(message.content, message.role, message.id, message.attachments, message.images, message.isRedacted, message.signature);
        LOG_MESSAGE(QString("Loaded message with %1 image(s), isRedacted=%2, signature length=%3")
                        .arg(message.images.size())
                        .arg(message.isRedacted)
                        .arg(message.signature.length()));
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

QString ChatSerializer::getChatImagesFolder(const QString &chatFilePath)
{
    QFileInfo fileInfo(chatFilePath);
    QString baseName = fileInfo.completeBaseName();
    QString dirPath = fileInfo.absolutePath();
    return QDir(dirPath).filePath(baseName + "_images");
}

bool ChatSerializer::saveImageToStorage(const QString &chatFilePath, 
                                        const QString &fileName,
                                        const QString &base64Data,
                                        QString &storedPath)
{
    QString imagesFolder = getChatImagesFolder(chatFilePath);
    QDir dir;
    if (!dir.exists(imagesFolder)) {
        if (!dir.mkpath(imagesFolder)) {
            LOG_MESSAGE(QString("Failed to create images folder: %1").arg(imagesFolder));
            return false;
        }
    }
    
    QFileInfo originalFileInfo(fileName);
    QString extension = originalFileInfo.suffix();
    QString baseName = originalFileInfo.completeBaseName();
    QString uniqueName = QString("%1_%2.%3")
                            .arg(baseName)
                            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8))
                            .arg(extension);
    
    QString fullPath = QDir(imagesFolder).filePath(uniqueName);
    
    QByteArray imageData = QByteArray::fromBase64(base64Data.toUtf8());
    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_MESSAGE(QString("Failed to open file for writing: %1").arg(fullPath));
        return false;
    }
    
    if (file.write(imageData) == -1) {
        LOG_MESSAGE(QString("Failed to write image data: %1").arg(file.errorString()));
        return false;
    }
    
    file.close();
    
    storedPath = uniqueName;
    LOG_MESSAGE(QString("Saved image: %1 to %2").arg(fileName, fullPath));
    
    return true;
}

QString ChatSerializer::loadImageFromStorage(const QString &chatFilePath, const QString &storedPath)
{
    QString imagesFolder = getChatImagesFolder(chatFilePath);
    QString fullPath = QDir(imagesFolder).filePath(storedPath);
    
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_MESSAGE(QString("Failed to open image file: %1").arg(fullPath));
        return QString();
    }
    
    QByteArray imageData = file.readAll();
    file.close();
    
    return imageData.toBase64();
}

} // namespace QodeAssist::Chat
