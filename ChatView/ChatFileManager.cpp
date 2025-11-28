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

#include "ChatFileManager.hpp"
#include "Logger.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
#include <QDateTime>
#include <QRegularExpression>

#include <coreplugin/icore.h>

namespace QodeAssist::Chat {

ChatFileManager::ChatFileManager(QObject *parent)
    : QObject(parent)
    , m_intermediateStorageDir(getIntermediateStorageDir())
{}

ChatFileManager::~ChatFileManager() = default;

QStringList ChatFileManager::processDroppedFiles(const QStringList &filePaths)
{
    QStringList processedPaths;
    processedPaths.reserve(filePaths.size());

    for (const QString &filePath : filePaths) {
        if (!isFileAccessible(filePath)) {
            const QString error = tr("File is not accessible: %1").arg(filePath);
            LOG_MESSAGE(error);
            emit fileOperationFailed(error);
            continue;
        }

        QString copiedPath = copyToIntermediateStorage(filePath);
        if (!copiedPath.isEmpty()) {
            processedPaths.append(copiedPath);
            emit fileCopiedToStorage(filePath, copiedPath);
            LOG_MESSAGE(QString("File copied to storage: %1 -> %2").arg(filePath, copiedPath));
        } else {
            const QString error = tr("Failed to copy file: %1").arg(filePath);
            LOG_MESSAGE(error);
            emit fileOperationFailed(error);
        }
    }

    return processedPaths;
}

void ChatFileManager::setChatFilePath(const QString &chatFilePath)
{
    m_chatFilePath = chatFilePath;
}

QString ChatFileManager::chatFilePath() const
{
    return m_chatFilePath;
}

void ChatFileManager::clearIntermediateStorage()
{
    QDir dir(m_intermediateStorageDir);
    if (!dir.exists()) {
        return;
    }

    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fileInfo : files) {
        QFile file(fileInfo.absoluteFilePath());
        file.setPermissions(QFile::WriteUser | QFile::ReadUser);
        if (file.remove()) {
            LOG_MESSAGE(QString("Removed intermediate file: %1").arg(fileInfo.fileName()));
        } else {
            LOG_MESSAGE(QString("Failed to remove intermediate file: %1")
                            .arg(fileInfo.fileName()));
        }
    }
}

bool ChatFileManager::isFileAccessible(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.exists() && fileInfo.isFile() && fileInfo.isReadable();
}

void ChatFileManager::cleanupGlobalIntermediateStorage()
{
    const QString basePath = Core::ICore::userResourcePath().toFSPathString();
    const QString intermediatePath = QDir(basePath).filePath("qodeassist/chat_temp_files");

    QDir dir(intermediatePath);
    if (!dir.exists()) {
        return;
    }

    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    int removedCount = 0;
    int failedCount = 0;

    for (const QFileInfo &fileInfo : files) {
        QFile file(fileInfo.absoluteFilePath());
        file.setPermissions(QFile::WriteUser | QFile::ReadUser);
        if (file.remove()) {
            removedCount++;
        } else {
            failedCount++;
        }
    }

    if (removedCount > 0 || failedCount > 0) {
        LOG_MESSAGE(QString("ChatFileManager global cleanup: removed=%1, failed=%2")
                        .arg(removedCount)
                        .arg(failedCount));
    }
}

QString ChatFileManager::copyToIntermediateStorage(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        LOG_MESSAGE(QString("Source file does not exist or is not a file: %1").arg(filePath));
        return QString();
    }

    if (fileInfo.size() == 0) {
        LOG_MESSAGE(QString("Source file is empty: %1").arg(filePath));
    }

    const QString newFileName = generateIntermediateFileName(filePath);
    const QString destinationPath = QDir(m_intermediateStorageDir).filePath(newFileName);

    if (QFileInfo::exists(destinationPath)) {
        QFile::remove(destinationPath);
    }

    if (!QFile::copy(filePath, destinationPath)) {
        LOG_MESSAGE(QString("Failed to copy file: %1 -> %2").arg(filePath, destinationPath));
        return QString();
    }

    QFile copiedFile(destinationPath);
    if (!copiedFile.exists()) {
        LOG_MESSAGE(QString("Copied file does not exist after copy: %1").arg(destinationPath));
        return QString();
    }

    copiedFile.setPermissions(QFile::ReadUser | QFile::WriteUser);

    return destinationPath;
}

QString ChatFileManager::getIntermediateStorageDir()
{
    const QString basePath = Core::ICore::userResourcePath().toFSPathString();
    const QString intermediatePath = QDir(basePath).filePath("qodeassist/chat_temp_files");

    QDir dir;
    if (!dir.exists(intermediatePath) && !dir.mkpath(intermediatePath)) {
        LOG_MESSAGE(QString("Failed to create intermediate storage directory: %1")
                        .arg(intermediatePath));
    }

    return intermediatePath;
}

QString ChatFileManager::generateIntermediateFileName(const QString &originalPath)
{
    const QFileInfo fileInfo(originalPath);
    const QString extension = fileInfo.suffix();
    QString baseName = fileInfo.completeBaseName().left(30);

    static const QRegularExpression specialChars("[^a-zA-Z0-9_-]");
    baseName.replace(specialChars, "_");

    if (baseName.isEmpty()) {
        baseName = "file";
    }

    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);

    return QString("%1_%2_%3.%4").arg(baseName, timestamp, uuid, extension);
}

} // namespace QodeAssist::Chat

