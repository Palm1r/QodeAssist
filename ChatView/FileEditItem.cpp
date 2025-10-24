/* 
 * Copyright (C) 2025 Petr Mironychev
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

#include "FileEditItem.hpp"

#include "Logger.hpp"
#include "settings/GeneralSettings.hpp"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QTextStream>
#include <QTimer>

namespace QodeAssist::Chat {

QMutex FileEditItem::s_fileLockMutex;
QSet<QString> FileEditItem::s_lockedFiles;

FileEditItem::FileEditItem(QQuickItem *parent)
    : QQuickItem(parent)
{}

void FileEditItem::parseFromContent(const QString &content)
{
    static const QLatin1String marker(EDIT_MARKER);
    int markerPos = content.indexOf(marker);

    if (markerPos == -1) {
        LOG_MESSAGE(QString("FileEditItem: ERROR - no marker found"));
        return;
    }

    int jsonStart = markerPos + marker.size();
    QString jsonStr = content.mid(jsonStart);

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        LOG_MESSAGE(QString("FileEditItem: JSON parse error at offset %1: %2")
                        .arg(parseError.offset)
                        .arg(parseError.errorString()));
        return;
    }

    if (!doc.isObject()) {
        LOG_MESSAGE(QString("FileEditItem: ERROR - parsed JSON is not an object"));
        return;
    }

    QJsonObject editData = doc.object();

    // Generate unique edit ID
    m_editId = QString("edit_%1").arg(QDateTime::currentMSecsSinceEpoch());
    m_filePath = editData["filepath"].toString();
    m_newText = editData["new_text"].toString();
    m_searchText = editData["search_text"].toString();

    // Calculate line counts
    m_addedLines = m_newText.split('\n').size();
    m_removedLines = m_searchText.isEmpty() ? 0 : m_searchText.split('\n').size();

    LOG_MESSAGE(QString("FileEditItem: parsed successfully, editId=%1, filePath=%2, searchText=%3")
                    .arg(m_editId, m_filePath, m_searchText.isEmpty() ? "empty (append mode)" : "present"));

    emit editIdChanged();
    emit filePathChanged();
    emit searchTextChanged();
    emit newTextChanged();
    emit addedLinesChanged();
    emit removedLinesChanged();

    bool autoApplyEnabled = Settings::generalSettings().autoApplyFileEdits.value();
    if (autoApplyEnabled) {
        applyEditInternal(true);
    }
}

void FileEditItem::applyEdit()
{
    applyEditInternal(false, 0);
}

void FileEditItem::applyEditInternal(bool isAutomatic, int retryCount)
{
    if (isAutomatic) {
        if (m_status != EditStatus::Pending) {
            return;
        }
    } else {
        if (m_status != EditStatus::Pending && m_status != EditStatus::Reverted 
            && m_status != EditStatus::Rejected) {
            return;
        }
    }

    if (!acquireFileLock(m_filePath)) {
        if (retryCount >= MAX_RETRY_COUNT) {
            rejectWithError(QString("File %1 is locked, exceeded retry limit").arg(m_filePath));
            return;
        }
        
        const int retryDelay = isAutomatic ? AUTO_APPLY_RETRY_DELAY_MS : RETRY_DELAY_MS;
        QTimer::singleShot(retryDelay, this, [this, isAutomatic, retryCount]() { 
            applyEditInternal(isAutomatic, retryCount + 1); 
        });
        return;
    }

    performApply();
    releaseFileLock(m_filePath);
}

void FileEditItem::revertEdit()
{
    if (m_status != EditStatus::Applied) {
        return;
    }

    if (!acquireFileLock(m_filePath)) {
        QTimer::singleShot(RETRY_DELAY_MS, this, &FileEditItem::revertEdit);
        return;
    }

    performRevert();
    releaseFileLock(m_filePath);
}

void FileEditItem::performApply()
{
    LOG_MESSAGE(QString("FileEditItem: applying edit %1 to %2").arg(m_editId, m_filePath));

    QString currentContent = readFile(m_filePath);

    m_originalContent = currentContent;

    QString editedContent;
    
    if (m_searchText.isEmpty()) {
        // Append mode
        editedContent = currentContent;
        if (!editedContent.endsWith('\n')) {
            editedContent += '\n';
        }
        editedContent += m_newText;
        if (!m_newText.endsWith('\n')) {
            editedContent += '\n';
        }
    } else {
        // Replace mode
        int pos = currentContent.indexOf(m_searchText);
        if (pos == -1) {
            rejectWithError("Failed to apply edit: search text not found. File may have been modified.");
            return;
        }
        editedContent = currentContent;
        editedContent.replace(pos, m_searchText.length(), m_newText);
    }

    if (!writeFile(m_filePath, editedContent)) {
        rejectWithError(QString("Failed to write file: %1").arg(m_filePath));
        return;
    }

    finishWithSuccess(EditStatus::Applied, QString("Successfully applied edit to: %1").arg(m_filePath));
}

void FileEditItem::performRevert()
{
    LOG_MESSAGE(QString("FileEditItem: reverting edit %1 for %2").arg(m_editId, m_filePath));

    if (!writeFile(m_filePath, m_originalContent)) {
        rejectWithError(QString("Failed to write reverted file: %1").arg(m_filePath));
        return;
    }

    finishWithSuccess(EditStatus::Reverted, QString("Successfully reverted edit to: %1").arg(m_filePath));
}

void FileEditItem::rejectWithError(const QString &errorMessage)
{
    LOG_MESSAGE(errorMessage);
    setStatus(EditStatus::Rejected);
    setStatusMessage(errorMessage);
}

void FileEditItem::finishWithSuccess(EditStatus status, const QString &message)
{
    LOG_MESSAGE(message);
    setStatus(status);
    setStatusMessage(message);
}

void FileEditItem::setStatus(EditStatus status)
{
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged();
}

void FileEditItem::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message)
        return;

    m_statusMessage = message;
    emit statusMessageChanged();
}

bool FileEditItem::writeFile(const QString &filePath, const QString &content)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("Could not open file for writing: %1").arg(filePath));
        return false;
    }

    QTextStream stream(&file);
    stream.setAutoDetectUnicode(true);
    stream << content;
    file.close();

    if (stream.status() != QTextStream::Ok) {
        LOG_MESSAGE(QString("Error writing to file: %1").arg(filePath));
        return false;
    }

    return true;
}

QString FileEditItem::readFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("Could not open file for reading: %1").arg(filePath));
        return QString();
    }

    QTextStream stream(&file);
    stream.setAutoDetectUnicode(true);
    QString content = stream.readAll();
    file.close();

    return content;
}

bool FileEditItem::acquireFileLock(const QString &filePath)
{
    QMutexLocker locker(&s_fileLockMutex);
    
    if (s_lockedFiles.contains(filePath)) {
        return false;
    }
    
    s_lockedFiles.insert(filePath);
    LOG_MESSAGE(QString("FileEditItem: acquired lock for %1").arg(filePath));
    return true;
}

void FileEditItem::releaseFileLock(const QString &filePath)
{
    QMutexLocker locker(&s_fileLockMutex);
    
    if (s_lockedFiles.remove(filePath)) {
        LOG_MESSAGE(QString("FileEditItem: released lock for %1").arg(filePath));
    }
}

} // namespace QodeAssist::Chat
