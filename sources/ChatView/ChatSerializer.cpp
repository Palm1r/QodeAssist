// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatSerializer.hpp"
#include "Logger.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include "session/HistorySerializer.hpp"

namespace QodeAssist::Chat {

SerializationResult ChatSerializer::saveToFile(
    const Session::ConversationHistory &history, const QString &filePath)
{
    if (!ensureDirectoryExists(filePath)) {
        return {false, "Failed to create directory structure"};
    }

    const QJsonObject root = Session::HistorySerializer::toJson(history);

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {false, QString("Failed to open file for writing: %1").arg(filePath)};
    }

    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) == -1) {
        return {false, QString("Failed to write to file: %1").arg(file.errorString())};
    }

    if (!file.commit()) {
        return {false, QString("Failed to save file: %1").arg(file.errorString())};
    }

    return {true, QString()};
}

SerializationResult ChatSerializer::loadFromFile(
    Session::ConversationHistory &history, const QString &filePath)
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

    const QJsonObject root = doc.object();
    const QString version = root["version"].toString();

    if (!Session::HistorySerializer::isSupportedVersion(version)) {
        return {false, QString("Unsupported version: %1").arg(version)};
    }

    int droppedBlocks = 0;
    const auto loaded = Session::HistorySerializer::fromJson(root, &droppedBlocks);
    if (!loaded) {
        return {false, QString("Failed to read chat history from: %1").arg(filePath)};
    }

    if (version != Session::HistorySerializer::currentVersion()) {
        LOG_MESSAGE(QString("Converted chat from format %1 to %2")
                        .arg(version, Session::HistorySerializer::currentVersion()));
    }

    history = *loaded;

    if (droppedBlocks > 0) {
        const QString warning
            = QString(
                  "%1 message part(s) in this chat could not be read and will be lost if "
                  "the chat is saved again")
                  .arg(droppedBlocks);
        LOG_MESSAGE(QString("%1: %2").arg(filePath, warning));
        return {true, QString(), warning};
    }

    return {true, QString(), QString()};
}

bool ChatSerializer::ensureDirectoryExists(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    return dir.exists() || dir.mkpath(".");
}

QString ChatSerializer::getChatContentFolder(const QString &chatFilePath)
{
    QFileInfo fileInfo(chatFilePath);
    QString baseName = fileInfo.completeBaseName();
    QString dirPath = fileInfo.absolutePath();
    return QDir(dirPath).filePath(baseName + "_content");
}

bool ChatSerializer::saveContentToStorage(
    const QString &chatFilePath,
    const QString &fileName,
    const QString &base64Data,
    QString &storedPath)
{
    QString contentFolder = getChatContentFolder(chatFilePath);
    QDir dir;
    if (!dir.exists(contentFolder)) {
        if (!dir.mkpath(contentFolder)) {
            LOG_MESSAGE(QString("Failed to create content folder: %1").arg(contentFolder));
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

    QString fullPath = QDir(contentFolder).filePath(uniqueName);

    QByteArray contentData = QByteArray::fromBase64(base64Data.toUtf8());
    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_MESSAGE(QString("Failed to open file for writing: %1").arg(fullPath));
        return false;
    }

    if (file.write(contentData) == -1) {
        LOG_MESSAGE(QString("Failed to write content data: %1").arg(file.errorString()));
        return false;
    }

    file.close();

    storedPath = uniqueName;
    LOG_MESSAGE(QString("Saved content: %1 to %2").arg(fileName, fullPath));

    return true;
}

QString ChatSerializer::loadContentFromStorage(const QString &chatFilePath, const QString &storedPath)
{
    QString contentFolder = getChatContentFolder(chatFilePath);
    QString fullPath = QDir(contentFolder).filePath(storedPath);

    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_MESSAGE(QString("Failed to open content file: %1").arg(fullPath));
        return QString();
    }

    QByteArray contentData = file.readAll();
    file.close();

    return contentData.toBase64();
}

} // namespace QodeAssist::Chat
