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
        return;
    }

    int jsonStart = markerPos + marker.size();
    QString jsonStr = content.mid(jsonStart);

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return;
    }

    if (!doc.isObject()) {
        return;
    }

    QJsonObject editData = doc.object();

    m_editId = QString("edit_%1").arg(QDateTime::currentMSecsSinceEpoch());
    m_mode = editData["mode"].toString();
    m_filePath = editData["filepath"].toString();
    m_newText = editData["new_text"].toString();
    m_searchText = editData["search_text"].toString();
    m_lineBefore = editData["line_before"].toString();
    m_lineAfter = editData["line_after"].toString();

    if (m_mode.isEmpty()) {
        m_mode = m_searchText.isEmpty() ? "insert_after" : "replace";
    }

    m_addedLines = m_newText.split('\n').size();
    m_removedLines = m_searchText.isEmpty() ? 0 : m_searchText.split('\n').size();

    emit editIdChanged();
    emit modeChanged();
    emit filePathChanged();
    emit searchTextChanged();
    emit newTextChanged();
    emit lineBeforeChanged();
    emit lineAfterChanged();
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
    QString currentContent = readFile(m_filePath);
    m_originalContent = currentContent;

    QString editedContent;
    
    if (m_mode == "insert_after") {
        if (m_lineBefore.isEmpty()) {
            editedContent = m_newText + currentContent;
        } else {
            QList<int> positions;
            int pos = 0;
            while ((pos = currentContent.indexOf(m_lineBefore, pos)) != -1) {
                positions.append(pos);
                pos += m_lineBefore.length();
            }
            
            if (positions.isEmpty()) {
                rejectWithError("Failed to apply edit: line_before not found");
                return;
            }
            
            int matchedPosition = -1;
            
            if (!m_lineAfter.isEmpty()) {
                for (int beforePos : positions) {
                    int afterPos = beforePos + m_lineBefore.length();
                    if (afterPos + m_lineAfter.length() <= currentContent.length()) {
                        QString actualAfter = currentContent.mid(afterPos, m_lineAfter.length());
                        if (actualAfter == m_lineAfter) {
                            matchedPosition = afterPos;
                            break;
                        }
                    }
                }
                
                if (matchedPosition == -1) {
                    rejectWithError("Failed to apply edit: line_before found but line_after context doesn't match");
                    return;
                }
            } else {
                matchedPosition = positions.first() + m_lineBefore.length();
            }
            
            editedContent = currentContent;
            editedContent.insert(matchedPosition, m_newText);
        }
        
    } else if (m_mode == "replace") {
        if (m_searchText.isEmpty()) {
            rejectWithError("REPLACE mode requires search_text to be specified");
            return;
        }
        
        bool hasLineBefore = !m_lineBefore.isEmpty();
        bool hasLineAfter = !m_lineAfter.isEmpty();
        
        QList<int> searchPositions;
        int pos = 0;
        while ((pos = currentContent.indexOf(m_searchText, pos)) != -1) {
            searchPositions.append(pos);
            pos += m_searchText.length();
        }
        
        if (searchPositions.isEmpty()) {
            rejectWithError("Failed to apply edit: search_text not found. File may have been modified.");
            return;
        }
        
        int matchedPosition = -1;
        const int MAX_CONTEXT_DISTANCE = 500;
        
        for (int searchPos : searchPositions) {
            bool beforeMatches = true;
            bool afterMatches = true;
            
            if (hasLineBefore) {
                int searchStart = qMax(0, searchPos - MAX_CONTEXT_DISTANCE);
                int beforePos = currentContent.lastIndexOf(m_lineBefore, searchPos - 1);
                
                if (beforePos >= searchStart && beforePos < searchPos) {
                    beforeMatches = true;
                } else {
                    beforeMatches = false;
                }
            }
            
            if (hasLineAfter) {
                int afterPos = searchPos + m_searchText.length();
                int searchEnd = qMin(currentContent.length(), afterPos + MAX_CONTEXT_DISTANCE);
                int foundAfterPos = currentContent.indexOf(m_lineAfter, afterPos);
                
                if (foundAfterPos >= afterPos && foundAfterPos < searchEnd) {
                    afterMatches = true;
                } else {
                    afterMatches = false;
                }
            }
            
            bool isMatch = false;
            
            if (hasLineBefore && hasLineAfter) {
                isMatch = beforeMatches && afterMatches;
            } else if (hasLineBefore && !hasLineAfter) {
                isMatch = beforeMatches;
            } else if (!hasLineBefore && hasLineAfter) {
                isMatch = afterMatches;
            } else {
                isMatch = true;
            }
            
            if (isMatch) {
                matchedPosition = searchPos;
                break;
            }
        }
        
        if (matchedPosition == -1) {
            rejectWithError("Failed to apply edit: search_text found but context verification failed.");
            return;
        }
        
        editedContent = currentContent;
        editedContent.replace(matchedPosition, m_searchText.length(), m_newText);
        
    } else {
        rejectWithError(QString("Unknown edit mode: %1").arg(m_mode));
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
    if (!writeFile(m_filePath, m_originalContent)) {
        rejectWithError(QString("Failed to write reverted file: %1").arg(m_filePath));
        return;
    }

    finishWithSuccess(EditStatus::Reverted, QString("Successfully reverted edit to: %1").arg(m_filePath));
}

void FileEditItem::rejectWithError(const QString &errorMessage)
{
    setStatus(EditStatus::Rejected);
    setStatusMessage(errorMessage);
}

void FileEditItem::finishWithSuccess(EditStatus status, const QString &message)
{
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
        return false;
    }

    QTextStream stream(&file);
    stream.setAutoDetectUnicode(true);
    stream << content;
    file.close();

    if (stream.status() != QTextStream::Ok) {
        return false;
    }

    return true;
}

QString FileEditItem::readFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
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
    return true;
}

void FileEditItem::releaseFileLock(const QString &filePath)
{
    QMutexLocker locker(&s_fileLockMutex);
    s_lockedFiles.remove(filePath);
}

} // namespace QodeAssist::Chat
