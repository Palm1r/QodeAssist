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

    m_editId = editData["edit_id"].toString();
    m_filePath = editData["file_path"].toString();
    m_editMode = editData["mode"].toString();
    m_originalContent = editData["original_content"].toString();
    m_newContent = editData["new_content"].toString();
    m_contextBefore = editData["context_before"].toString();
    m_contextAfter = editData["context_after"].toString();
    m_searchText = editData["search_text"].toString();
    m_lineNumber = editData["line_number"].toInt(-1);

    m_addedLines = m_newContent.split('\n').size();
    m_removedLines = m_originalContent.split('\n').size();

    LOG_MESSAGE(QString("FileEditItem: parsed successfully, editId=%1, filePath=%2")
                    .arg(m_editId, m_filePath));

    emit editIdChanged();
    emit filePathChanged();
    emit editModeChanged();
    emit originalContentChanged();
    emit newContentChanged();
    emit addedLinesChanged();
    emit removedLinesChanged();

    applyEditInternal(true);
}

void FileEditItem::applyEdit()
{
    applyEditInternal(false, 0);
}

void FileEditItem::applyEditInternal(bool isAutomatic, int retryCount)
{
    if (!isAutomatic && m_status != EditStatus::Reverted && m_status != EditStatus::Rejected) {
        return;
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
    if (currentContent.isNull()) {
        rejectWithError(QString("Failed to read file: %1").arg(m_filePath));
        return;
    }

    bool success = false;
    QString editedContent = applyEditToContent(currentContent, success);
    if (!success) {
        rejectWithError("Failed to apply edit: could not find context. File may have been modified.");
        return;
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

    QString currentContent = readFile(m_filePath);
    if (currentContent.isNull()) {
        rejectWithError(QString("Failed to read file for revert: %1").arg(m_filePath));
        return;
    }

    bool success = false;
    QString revertedContent = applyReverseEdit(currentContent, success);
    if (!success) {
        rejectWithError("Failed to revert edit: could not find changes in current file.");
        return;
    }

    if (!writeFile(m_filePath, revertedContent)) {
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

QString FileEditItem::applyEditToContent(const QString &content, bool &success)
{
    success = false;
    QStringList lines = content.split('\n');

    if (m_editMode == "replace") {
        QString searchPattern = m_contextBefore + m_searchText + m_contextAfter;
        int pos = content.indexOf(searchPattern);

        if (pos == -1 && !m_contextBefore.isEmpty()) {
            pos = content.indexOf(m_searchText);
        }

        if (pos != -1) {
            QString result = content;
            int searchPos = result.indexOf(m_searchText, pos);
            if (searchPos != -1) {
                result.replace(searchPos, m_searchText.length(), m_newContent);
                success = true;
                return result;
            }
        }

        return content;

    } else if (m_editMode == "insert_before" || m_editMode == "insert_after") {
        int targetLine = -1;

        if (!m_contextBefore.isEmpty() || !m_contextAfter.isEmpty()) {
            for (int i = 0; i < lines.size(); ++i) {
                bool contextMatches = true;

                if (!m_contextBefore.isEmpty()) {
                    QStringList beforeLines = m_contextBefore.split('\n');
                    if (i >= beforeLines.size()) {
                        bool allMatch = true;
                        for (int j = 0; j < beforeLines.size(); ++j) {
                            if (lines[i - beforeLines.size() + j].trimmed()
                                != beforeLines[j].trimmed()) {
                                allMatch = false;
                                break;
                            }
                        }
                        if (!allMatch)
                            contextMatches = false;
                    } else {
                        contextMatches = false;
                    }
                }

                if (contextMatches && !m_contextAfter.isEmpty()) {
                    QStringList afterLines = m_contextAfter.split('\n');
                    if (i + afterLines.size() < lines.size()) {
                        bool allMatch = true;
                        for (int j = 0; j < afterLines.size(); ++j) {
                            if (lines[i + 1 + j].trimmed() != afterLines[j].trimmed()) {
                                allMatch = false;
                                break;
                            }
                        }
                        if (!allMatch)
                            contextMatches = false;
                    } else {
                        contextMatches = false;
                    }
                }

                if (contextMatches && targetLine == -1) {
                    targetLine = i;
                    break;
                }
            }
        }

        if (targetLine == -1 && m_lineNumber > 0 && m_lineNumber <= lines.size()) {
            targetLine = m_lineNumber - 1;
        }

        if (targetLine != -1) {
            if (m_editMode == "insert_before") {
                lines.insert(targetLine, m_newContent);
            } else {
                lines.insert(targetLine + 1, m_newContent);
            }
            success = true;
            return lines.join('\n');
        }

        return content;

    } else if (m_editMode == "append") {
        success = true;
        return content + (content.endsWith('\n') ? "" : "\n") + m_newContent + "\n";
    }

    return content;
}

QString FileEditItem::applyReverseEdit(const QString &content, bool &success)
{
    success = false;
    QStringList lines = content.split('\n');

    if (m_editMode == "replace") {
        int pos = content.indexOf(m_newContent);
        if (pos != -1) {
            QString result = content;
            result.replace(pos, m_newContent.length(), m_originalContent);
            success = true;
            return result;
        }
        return content;

    } else if (m_editMode == "insert_before" || m_editMode == "insert_after") {
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].trimmed() == m_newContent.trimmed()) {
                bool contextMatches = true;
                
                if (!m_contextBefore.isEmpty()) {
                    QStringList beforeLines = m_contextBefore.split('\n');
                    if (i >= beforeLines.size()) {
                        for (int j = 0; j < beforeLines.size(); ++j) {
                            if (lines[i - beforeLines.size() + j].trimmed() != beforeLines[j].trimmed()) {
                                contextMatches = false;
                                break;
                            }
                        }
                    } else {
                        contextMatches = false;
                    }
                }

                if (contextMatches && !m_contextAfter.isEmpty()) {
                    QStringList afterLines = m_contextAfter.split('\n');
                    if (i + 1 + afterLines.size() <= lines.size()) {
                        for (int j = 0; j < afterLines.size(); ++j) {
                            if (lines[i + 1 + j].trimmed() != afterLines[j].trimmed()) {
                                contextMatches = false;
                                break;
                            }
                        }
                    } else {
                        contextMatches = false;
                    }
                }

                if (contextMatches) {
                    lines.removeAt(i);
                    success = true;
                    return lines.join('\n');
                }
            }
        }
        return content;

    } else if (m_editMode == "append") {
        QString suffix1 = m_newContent + "\n";
        QString suffix2 = "\n" + m_newContent + "\n";
        
        if (content.endsWith(suffix1)) {
            QString result = content.left(content.length() - suffix1.length());
            success = true;
            return result;
        } else if (content.endsWith(suffix2)) {
            QString result = content.left(content.length() - suffix2.length()) + "\n";
            success = true;
            return result;
        } else if (content.endsWith(m_newContent)) {
            QString result = content.left(content.length() - m_newContent.length());
            success = true;
            return result;
        }
        return content;
    }

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
