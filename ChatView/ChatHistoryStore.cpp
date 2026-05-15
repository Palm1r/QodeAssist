// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatHistoryStore.hpp"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

#include <coreplugin/icore.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include "ChatModel.hpp"
#include "Logger.hpp"
#include "ProjectSettings.hpp"

namespace QodeAssist::Chat {

ChatHistoryStore::ChatHistoryStore(ChatModel *chatModel, QObject *parent)
    : QObject(parent)
    , m_chatModel(chatModel)
{}

QString ChatHistoryStore::historyDir() const
{
    QString path;

    if (auto project = ProjectExplorer::ProjectManager::startupProject()) {
        Settings::ProjectSettings projectSettings(project);
        path = projectSettings.chatHistoryPath().toFSPathString();
    } else {
        QDir baseDir(Core::ICore::userResourcePath().toFSPathString());
        path = baseDir.filePath("qodeassist/chat_history");
    }

    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(".")) {
        LOG_MESSAGE(QString("Failed to create directory: %1").arg(path));
        return QString();
    }

    return path;
}

QString ChatHistoryStore::suggestedFileName() const
{
    QString shortMessage;

    if (m_chatModel->rowCount() > 0) {
        QString firstMessage
            = m_chatModel->data(m_chatModel->index(0), ChatModel::Content).toString();
        shortMessage = firstMessage.split('\n').first().simplified().left(30);

        if (shortMessage.isEmpty()) {
            QVariantList images
                = m_chatModel->data(m_chatModel->index(0), ChatModel::Images).toList();
            if (!images.isEmpty()) {
                shortMessage = "image_chat";
            }
        }
    }

    return generateChatFileName(shortMessage, historyDir());
}

QString ChatHistoryStore::autosaveFilePath(const QString &recentFilePath) const
{
    if (!recentFilePath.isEmpty()) {
        return recentFilePath;
    }

    QString dir = historyDir();
    if (dir.isEmpty()) {
        return QString();
    }

    return QDir(dir).filePath(suggestedFileName() + ".json");
}

QString ChatHistoryStore::autosaveFilePath(
    const QString &recentFilePath, const QString &firstMessage, bool hasImageAttachments) const
{
    if (!recentFilePath.isEmpty()) {
        return recentFilePath;
    }

    QString dir = historyDir();
    if (dir.isEmpty()) {
        return QString();
    }

    QString shortMessage = firstMessage.split('\n').first().simplified().left(30);

    if (shortMessage.isEmpty() && hasImageAttachments) {
        shortMessage = "image_chat";
    }

    QString fileName = generateChatFileName(shortMessage, dir);
    return QDir(dir).filePath(fileName + ".json");
}

SerializationResult ChatHistoryStore::save(const QString &filePath) const
{
    return ChatSerializer::saveToFile(m_chatModel, filePath);
}

SerializationResult ChatHistoryStore::load(const QString &filePath) const
{
    return ChatSerializer::loadFromFile(m_chatModel, filePath);
}

void ChatHistoryStore::showSaveDialog()
{
    QString initialDir = historyDir();

    QFileDialog *dialog = new QFileDialog(nullptr, tr("Save Chat History"));
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setNameFilter(tr("JSON files (*.json)"));
    dialog->setDefaultSuffix("json");
    if (!initialDir.isEmpty()) {
        dialog->setDirectory(initialDir);
        dialog->selectFile(suggestedFileName() + ".json");
    }

    connect(dialog, &QFileDialog::finished, this, [this, dialog](int result) {
        if (result == QFileDialog::Accepted) {
            QStringList files = dialog->selectedFiles();
            if (!files.isEmpty()) {
                emit saveRequested(files.first());
            }
        }
        dialog->deleteLater();
    });

    dialog->open();
}

void ChatHistoryStore::showLoadDialog()
{
    QString initialDir = historyDir();

    QFileDialog *dialog = new QFileDialog(nullptr, tr("Load Chat History"));
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setNameFilter(tr("JSON files (*.json)"));
    if (!initialDir.isEmpty()) {
        dialog->setDirectory(initialDir);
    }

    connect(dialog, &QFileDialog::finished, this, [this, dialog](int result) {
        if (result == QFileDialog::Accepted) {
            QStringList files = dialog->selectedFiles();
            if (!files.isEmpty()) {
                emit loadRequested(files.first());
            }
        }
        dialog->deleteLater();
    });

    dialog->open();
}

void ChatHistoryStore::openHistoryFolder() const
{
    QString path;
    if (auto project = ProjectExplorer::ProjectManager::startupProject()) {
        Settings::ProjectSettings projectSettings(project);
        path = projectSettings.chatHistoryPath().toFSPathString();
    } else {
        QDir baseDir(Core::ICore::userResourcePath().toFSPathString());
        path = baseDir.filePath("qodeassist/chat_history");
    }

    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QUrl url = QUrl::fromLocalFile(dir.absolutePath());
    QDesktopServices::openUrl(url);
}

QString ChatHistoryStore::generateChatFileName(const QString &shortMessage, const QString &dir) const
{
    static const QRegularExpression saitizeSymbols = QRegularExpression("[\\/:*?\"<>|\\s]");
    static const QRegularExpression underSymbols = QRegularExpression("_+");

    QStringList parts;
    QString sanitizedMessage = shortMessage;
    sanitizedMessage.replace(saitizeSymbols, "_");
    sanitizedMessage.replace(underSymbols, "_");
    sanitizedMessage = sanitizedMessage.trimmed();

    if (!sanitizedMessage.isEmpty()) {
        if (sanitizedMessage.startsWith('_')) {
            sanitizedMessage.remove(0, 1);
        }
        if (sanitizedMessage.endsWith('_')) {
            sanitizedMessage.chop(1);
        }

        QString fullPath = QDir(dir).filePath(sanitizedMessage);
        QFileInfo fileInfo(fullPath);
        if (!fileInfo.exists() && QFileInfo(fileInfo.path()).isWritable()) {
            parts << sanitizedMessage;
        }
    }

    parts << QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm");

    QString fileName = parts.join("_");
    QString fullPath = QDir(dir).filePath(fileName);
    QFileInfo finalCheck(fullPath);

    if (fileName.isEmpty() || finalCheck.exists() || !QFileInfo(finalCheck.path()).isWritable()) {
        fileName = QString("chat_%1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm"));
    }

    return fileName;
}

} // namespace QodeAssist::Chat
