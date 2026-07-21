// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatFileStore.hpp"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>
#include <QUuid>

#include <coreplugin/icore.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include "Logger.hpp"
#include "ProjectSettings.hpp"
#include "session/HistorySerializer.hpp"
#include "session/Session.hpp"

namespace QodeAssist::Chat {

ChatFileStore::ChatFileStore(Session::Session *session, QObject *parent)
    : QObject(parent)
    , m_session(session)
{}

QString ChatFileStore::historyDir() const
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

QString ChatFileStore::suggestedFileName() const
{
    QString shortMessage;

    if (!m_session)
        return generateChatFileName(shortMessage, historyDir());

    const QList<Session::MessageRow> &rows = m_session->rows();
    if (!rows.isEmpty()) {
        shortMessage = rows.first().content.split('\n').first().simplified().left(30);

        if (shortMessage.isEmpty() && !rows.first().images.isEmpty())
            shortMessage = "image_chat";
    }

    return generateChatFileName(shortMessage, historyDir());
}

QString ChatFileStore::autosaveFilePath(const QString &recentFilePath) const
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

QString ChatFileStore::autosaveFilePath(
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

void ChatFileStore::setBindingReader(BindingReader reader)
{
    m_bindingReader = std::move(reader);
}

void ChatFileStore::setBindingWriter(BindingWriter writer)
{
    m_bindingWriter = std::move(writer);
}

SerializationResult ChatFileStore::save(const QString &filePath) const
{
    if (!m_session)
        return {false, QString("Chat session is no longer available")};

    const Acp::AgentBinding binding = m_bindingReader ? m_bindingReader() : Acp::AgentBinding{};
    return saveToFile(m_session->history(), binding, filePath);
}

SerializationResult ChatFileStore::load(const QString &filePath) const
{
    if (!m_session)
        return {false, QString("Chat session is no longer available")};

    Session::ConversationHistory history;
    Acp::AgentBinding binding;
    const SerializationResult result = loadFromFile(history, binding, filePath);
    if (!result.success)
        return result;

    m_session->setHistory(history);
    if (m_bindingWriter)
        m_bindingWriter(binding);

    return result;
}

void ChatFileStore::showSaveDialog()
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

void ChatFileStore::showLoadDialog()
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

void ChatFileStore::openHistoryFolder() const
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

QString ChatFileStore::generateChatFileName(const QString &shortMessage, const QString &dir) const
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

SerializationResult ChatFileStore::saveToFile(
    const Session::ConversationHistory &history,
    const Acp::AgentBinding &binding,
    const QString &filePath)
{
    if (!ensureDirectoryExists(filePath)) {
        return {false, "Failed to create directory structure"};
    }

    QJsonObject root = Session::HistorySerializer::toJson(history);
    if (!binding.isEmpty())
        root["agent"] = binding.toJson();

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

SerializationResult ChatFileStore::loadFromFile(
    Session::ConversationHistory &history, Acp::AgentBinding &binding, const QString &filePath)
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

    QString bindingError;
    binding = Acp::AgentBinding::fromJson(root["agent"], &bindingError);
    if (!bindingError.isEmpty()) {
        const QString warning
            = QString("This chat records which agent held it, but %1, so it opens unbound")
                  .arg(bindingError);
        LOG_MESSAGE(QString("%1: %2").arg(filePath, warning));
        return {true, QString(), warning};
    }

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

bool ChatFileStore::ensureDirectoryExists(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    return dir.exists() || dir.mkpath(".");
}

QString ChatFileStore::getChatContentFolder(const QString &chatFilePath)
{
    QFileInfo fileInfo(chatFilePath);
    QString baseName = fileInfo.completeBaseName();
    QString dirPath = fileInfo.absolutePath();
    return QDir(dirPath).filePath(baseName + "_content");
}

bool ChatFileStore::saveContentToStorage(
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

QByteArray ChatFileStore::loadRawContentFromStorage(
    const QString &chatFilePath, const QString &storedPath)
{
    QString contentFolder = getChatContentFolder(chatFilePath);
    QString fullPath = QDir(contentFolder).filePath(storedPath);

    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_MESSAGE(QString("Failed to open content file: %1").arg(fullPath));
        return QByteArray();
    }

    QByteArray contentData = file.readAll();
    file.close();

    return contentData;
}

QString ChatFileStore::loadContentFromStorage(
    const QString &chatFilePath, const QString &storedPath)
{
    return QString::fromLatin1(loadRawContentFromStorage(chatFilePath, storedPath).toBase64());
}

} // namespace QodeAssist::Chat
