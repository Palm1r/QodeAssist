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

#include "ChangesManager.h"
#include "CodeCompletionSettings.hpp"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <logger/Logger.hpp>
#include <algorithm>
#include <QFile>
#include <QTextCursor>
#include <QTextStream>

namespace QodeAssist::Context {

ChangesManager &ChangesManager::instance()
{
    static ChangesManager instance;
    return instance;
}

ChangesManager::ChangesManager()
    : QObject(nullptr)
    , m_undoStack(new QUndoStack(this))
{}

ChangesManager::~ChangesManager() {}

void ChangesManager::addChange(
    TextEditor::TextDocument *document, int position, int charsRemoved, int charsAdded)
{
    auto &documentQueue = m_documentChanges[document];

    QTextBlock block = document->document()->findBlock(position);
    int lineNumber = block.blockNumber();
    QString lineContent = block.text();
    QString fileName = document->filePath().fileName();

    ChangeInfo change{fileName, lineNumber, lineContent};

    auto it
        = std::find_if(documentQueue.begin(), documentQueue.end(), [lineNumber](const ChangeInfo &c) {
              return c.lineNumber == lineNumber;
          });

    if (it != documentQueue.end()) {
        it->lineContent = lineContent;
    } else {
        documentQueue.enqueue(change);

        if (documentQueue.size() > Settings::codeCompletionSettings().maxChangesCacheSize()) {
            documentQueue.dequeue();
        }
    }
}

QString ChangesManager::getRecentChangesContext(const TextEditor::TextDocument *currentDocument) const
{
    QString context;
    for (auto it = m_documentChanges.constBegin(); it != m_documentChanges.constEnd(); ++it) {
        if (it.key() != currentDocument) {
            for (const auto &change : it.value()) {
                context += change.lineContent + "\n";
            }
        }
    }
    return context;
}

void ChangesManager::addFileEdit(
    const QString &editId,
    const QString &filePath,
    const QString &oldContent,
    const QString &newContent,
    bool autoApply,
    bool isFromHistory,
    const QString &requestId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_fileEdits.contains(editId)) {
        LOG_MESSAGE(QString("File edit already exists, skipping: %1").arg(editId));
        return;
    }

    FileEdit edit;
    edit.editId = editId;
    edit.filePath = filePath;
    edit.oldContent = oldContent;
    edit.newContent = newContent;
    edit.timestamp = QDateTime::currentDateTime();
    edit.wasAutoApplied = false;
    edit.isFromHistory = isFromHistory;

    LOG_MESSAGE(QString("Creating diff for edit %1").arg(editId));
    locker.unlock();
    edit.diffInfo = createDiffInfo(oldContent, newContent, filePath);
    locker.relock();
    LOG_MESSAGE(QString("Diff created for edit %1: %2 hunk(s), fallback: %3")
                   .arg(editId)
                   .arg(edit.diffInfo.hunks.size())
                   .arg(edit.diffInfo.useFallback ? "yes" : "no"));

    if (isFromHistory) {
        edit.status = Archived;
        edit.statusMessage = "Loaded from chat history";
        autoApply = false;
    } else {
        edit.status = Pending;
        edit.statusMessage = "Waiting to be applied";
    }

    m_fileEdits.insert(editId, edit);
    
    if (!requestId.isEmpty() && !isFromHistory) {
        if (!m_requestEdits.contains(requestId)) {
            m_requestEdits[requestId] = RequestEdits();
        }
        m_requestEdits[requestId].editIds.append(editId);
        
        LOG_MESSAGE(QString("File edit tracked for request: %1 (requestId: %2)")
                        .arg(editId, requestId));
    }
    
    emit fileEditAdded(editId);

    LOG_MESSAGE(QString("File edit added: %1 for file %2 (history: %3, autoApply: %4)")
                    .arg(editId, filePath, isFromHistory ? "yes" : "no", autoApply ? "yes" : "no"));

    if (autoApply && !edit.wasAutoApplied && !isFromHistory) {
        locker.unlock();
        if (applyFileEdit(editId)) {
            QMutexLocker relock(&m_mutex);
            m_fileEdits[editId].wasAutoApplied = true;
            LOG_MESSAGE(QString("File edit auto-applied immediately: %1").arg(editId));
        }
    }
}

bool ChangesManager::applyFileEdit(const QString &editId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_fileEdits.contains(editId)) {
        LOG_MESSAGE(QString("File edit not found: %1").arg(editId));
        return false;
    }

    FileEdit &edit = m_fileEdits[editId];

    if (edit.status == Applied) {
        LOG_MESSAGE(QString("File edit already applied: %1").arg(editId));
        return true;
    }

    if (edit.status == Archived) {
        LOG_MESSAGE(QString("Cannot apply archived file edit: %1").arg(editId));
        edit.statusMessage = "Cannot apply archived edit from history";
        return false;
    }

    QString filePathCopy = edit.filePath;
    QString oldContentCopy = edit.oldContent;
    QString newContentCopy = edit.newContent;
    
    locker.unlock();
    
    LOG_MESSAGE(QString("Applying edit %1 using fragment replacement").arg(editId));
    
    QString errorMsg;
    bool isAppend = oldContentCopy.isEmpty();
    bool success = performFragmentReplacement(
        filePathCopy, oldContentCopy, newContentCopy, isAppend, &errorMsg);
    
    locker.relock();
    
    if (success) {
        edit.status = Applied;
        edit.statusMessage = errorMsg.isEmpty() ? "Successfully applied" : errorMsg;
        
        locker.unlock();
        emit fileEditApplied(editId);
        
        LOG_MESSAGE(QString("File edit applied successfully: %1").arg(editId));
        return true;
    } else {
        edit.statusMessage = errorMsg.isEmpty() ? "Failed to apply" : errorMsg;
        LOG_MESSAGE(QString("File edit failed: %1 - %2").arg(editId, edit.statusMessage));
    }

    return false;
}

bool ChangesManager::rejectFileEdit(const QString &editId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_fileEdits.contains(editId)) {
        LOG_MESSAGE(QString("File edit not found: %1").arg(editId));
        return false;
    }

    FileEdit &edit = m_fileEdits[editId];

    if (edit.status == Archived) {
        LOG_MESSAGE(QString("Cannot reject archived file edit: %1").arg(editId));
        edit.statusMessage = "Cannot reject archived edit from history";
        return false;
    }

    edit.status = Rejected;
    edit.statusMessage = "Rejected by user";
    
    locker.unlock();
    emit fileEditRejected(editId);
    
    LOG_MESSAGE(QString("File edit rejected: %1").arg(editId));
    return true;
}

bool ChangesManager::undoFileEdit(const QString &editId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_fileEdits.contains(editId)) {
        LOG_MESSAGE(QString("File edit not found: %1").arg(editId));
        return false;
    }

    FileEdit &edit = m_fileEdits[editId];

    if (edit.status == Archived) {
        LOG_MESSAGE(QString("Cannot undo archived file edit: %1").arg(editId));
        edit.statusMessage = "Cannot undo archived edit from history";
        return false;
    }

    if (edit.status != Applied) {
        LOG_MESSAGE(QString("File edit is not applied, cannot undo: %1").arg(editId));
        edit.statusMessage = "Edit must be applied before it can be undone";
        return false;
    }

    QString filePathCopy = edit.filePath;
    QString oldContentCopy = edit.oldContent;
    QString newContentCopy = edit.newContent;
    
    locker.unlock();
    
    LOG_MESSAGE(QString("Undoing edit %1 using REVERSE fragment replacement").arg(editId));
    
    QString errorMsg;
    bool isAppend = oldContentCopy.isEmpty();
    bool success = performFragmentReplacement(
        filePathCopy, newContentCopy, oldContentCopy, isAppend, &errorMsg);
    
    locker.relock();
    
    if (success) {
        edit.status = Rejected;
        edit.statusMessage = errorMsg.isEmpty() ? "Successfully undone" : errorMsg;
        edit.wasAutoApplied = false;
        
        locker.unlock();
        emit fileEditUndone(editId);
        
        LOG_MESSAGE(QString("File edit undone successfully: %1").arg(editId));
        return true;
    } else {
        edit.statusMessage = errorMsg.isEmpty() ? "Failed to undo" : errorMsg;
        LOG_MESSAGE(QString("File edit undo failed: %1 - %2").arg(editId, edit.statusMessage));
    }

    return false;
}

ChangesManager::FileEdit ChangesManager::getFileEdit(const QString &editId) const
{
    QMutexLocker locker(&m_mutex);
    return m_fileEdits.value(editId);
}

QList<ChangesManager::FileEdit> ChangesManager::getPendingEdits() const
{
    QMutexLocker locker(&m_mutex);
    
    QList<FileEdit> pendingEdits;
    for (const auto &edit : m_fileEdits.values()) {
        if (edit.status == Pending) {
            pendingEdits.append(edit);
        }
    }
    return pendingEdits;
}

bool ChangesManager::performFileEdit(
    const QString &filePath, const QString &oldContent, const QString &newContent, QString *errorMsg)
{
    auto setError = [errorMsg](const QString &msg) {
        if (errorMsg) *errorMsg = msg;
    };

    auto editors = Core::EditorManager::visibleEditors();
    for (auto *editor : editors) {
        if (!editor || !editor->document()) {
            continue;
        }

        QString editorPath = editor->document()->filePath().toFSPathString();
        if (editorPath == filePath) {
            QByteArray contentBytes = editor->document()->contents();
            QString currentContent = QString::fromUtf8(contentBytes);

            if (oldContent.isEmpty()) {
                if (auto *textEditor
                    = qobject_cast<TextEditor::TextDocument *>(editor->document())) {
                    QTextDocument *doc = textEditor->document();
                    
                    QTextCursor cursor(doc);
                    cursor.beginEditBlock();
                    cursor.movePosition(QTextCursor::End);
                    cursor.insertText(newContent);
                    cursor.endEditBlock();
                    
                    LOG_MESSAGE(QString("Appended to open editor: %1").arg(filePath));
                    setError("Applied successfully (appended to end of file)");
                    return true;
                }
            }

            int matchPos = currentContent.indexOf(oldContent);
            if (matchPos != -1) {
                if (auto *textEditor
                    = qobject_cast<TextEditor::TextDocument *>(editor->document())) {
                    QTextDocument *doc = textEditor->document();
                    
                    QTextCursor cursor(doc);
                    cursor.beginEditBlock();
                    cursor.setPosition(matchPos);
                    cursor.setPosition(matchPos + oldContent.length(), QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                    cursor.insertText(newContent);
                    cursor.endEditBlock();
                    
                    LOG_MESSAGE(QString("Updated open editor (exact match): %1").arg(filePath));
                    setError("Applied successfully (exact match)");
                    return true;
                }
            } else {
                double similarity = 0.0;
                QString matchedContent = findBestMatch(currentContent, oldContent, 0.8, &similarity);
                if (!matchedContent.isEmpty()) {
                    matchPos = currentContent.indexOf(matchedContent);
                    if (matchPos != -1) {
                        if (auto *textEditor
                            = qobject_cast<TextEditor::TextDocument *>(editor->document())) {
                            QTextDocument *doc = textEditor->document();
                            
                            QTextCursor cursor(doc);
                            cursor.beginEditBlock();
                            cursor.setPosition(matchPos);
                            cursor.setPosition(matchPos + matchedContent.length(), QTextCursor::KeepAnchor);
                            cursor.removeSelectedText();
                            cursor.insertText(newContent);
                            cursor.endEditBlock();
                            
                            LOG_MESSAGE(QString("Updated open editor (fuzzy match %1%%): %2")
                                            .arg(qRound(similarity * 100)).arg(filePath));
                            setError(QString("Applied with fuzzy match (%1%% similarity)").arg(qRound(similarity * 100)));
                            return true;
                        }
                    }
                }
                
                LOG_MESSAGE(QString("Old content not found in open editor (best similarity: %1%%): %2")
                                .arg(qRound(similarity * 100)).arg(filePath));
                setError(QString("Content not found. Best match: %1%% (threshold: 80%%). "
                                "File may have changed.").arg(qRound(similarity * 100)));
                return false;
            }
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString msg = QString("Cannot open file: %1").arg(file.errorString());
        LOG_MESSAGE(QString("Failed to open file for reading: %1 - %2").arg(filePath, file.errorString()));
        setError(msg);
        return false;
    }

    QString currentContent = QString::fromUtf8(file.readAll());
    file.close();

    QString updatedContent;
    
    if (oldContent.isEmpty()) {
        updatedContent = currentContent + newContent;
        LOG_MESSAGE(QString("Appending to file: %1").arg(filePath));
        setError("Applied successfully (appended to end of file)");
    }
    else if (currentContent.contains(oldContent)) {
        updatedContent = currentContent.replace(oldContent, newContent);
        LOG_MESSAGE(QString("Using exact match for file update: %1").arg(filePath));
        setError("Applied successfully (exact match)");
    } else {
        double similarity = 0.0;
        QString matchedContent = findBestMatch(currentContent, oldContent, 0.8, &similarity);
        if (!matchedContent.isEmpty()) {
            updatedContent = currentContent.replace(matchedContent, newContent);
            LOG_MESSAGE(QString("Using fuzzy match (%1%%) for file update: %2")
                            .arg(qRound(similarity * 100)).arg(filePath));
            setError(QString("Applied with fuzzy match (%1%% similarity)").arg(qRound(similarity * 100)));
        } else {
            QString msg = QString("Content not found. Best match: %1%% (threshold: 80%%). "
                                 "File may have changed.").arg(qRound(similarity * 100));
            LOG_MESSAGE(QString("Old content not found in file (best similarity: %1%%): %2")
                            .arg(qRound(similarity * 100)).arg(filePath));
            setError(msg);
            return false;
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QString msg = QString("Cannot write file: %1").arg(file.errorString());
        LOG_MESSAGE(QString("Failed to open file for writing: %1 - %2").arg(filePath, file.errorString()));
        setError(msg);
        return false;
    }

    QTextStream out(&file);
    out << updatedContent;
    file.close();

    LOG_MESSAGE(QString("File updated: %1").arg(filePath));
    return true;
}

int ChangesManager::levenshteinDistance(const QString &s1, const QString &s2) const
{
    const int len1 = s1.length();
    const int len2 = s2.length();
    
    QVector<QVector<int>> d(len1 + 1, QVector<int>(len2 + 1));
    
    for (int i = 0; i <= len1; ++i) {
        d[i][0] = i;
    }
    for (int j = 0; j <= len2; ++j) {
        d[0][j] = j;
    }
    
    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = std::min({
                d[i - 1][j] + 1,
                d[i][j - 1] + 1,
                d[i - 1][j - 1] + cost
            });
        }
    }
    
    return d[len1][len2];
}

QString ChangesManager::findBestMatch(const QString &fileContent, const QString &searchContent, double threshold, double *outSimilarity) const
{
    if (searchContent.isEmpty() || fileContent.isEmpty()) {
        if (outSimilarity) *outSimilarity = 0.0;
        return QString();
    }
    
    const int searchLen = searchContent.length();
    const int fileLen = fileContent.length();
    
    if (searchLen > fileLen) {
        if (outSimilarity) *outSimilarity = 0.0;
        return QString();
    }
    
    QString bestMatch;
    double bestSimilarity = 0.0;
    
    for (int i = 0; i <= fileLen - searchLen; ++i) {
        QString candidate = fileContent.mid(i, searchLen);
        int distance = levenshteinDistance(candidate, searchContent);
        double similarity = 1.0 - (static_cast<double>(distance) / searchLen);
        
        if (similarity > bestSimilarity) {
            bestSimilarity = similarity;
            if (similarity >= threshold) {
                bestMatch = candidate;
            }
        }
    }
    
    if (outSimilarity) {
        *outSimilarity = bestSimilarity;
    }
    
    if (!bestMatch.isEmpty()) {
        LOG_MESSAGE(QString("Fuzzy match found with similarity: %1%%").arg(qRound(bestSimilarity * 100)));
    } else {
        LOG_MESSAGE(QString("No match found above threshold. Best similarity: %1%%").arg(qRound(bestSimilarity * 100)));
    }
    
    return bestMatch;
}

QString ChangesManager::findBestMatchWithNormalization(
    const QString &fileContent, 
    const QString &searchContent, 
    double *outSimilarity,
    QString *outMatchType) const
{
    if (searchContent.isEmpty() || fileContent.isEmpty()) {
        if (outSimilarity) *outSimilarity = 0.0;
        if (outMatchType) *outMatchType = "none";
        return QString();
    }
    
    if (fileContent.contains(searchContent)) {
        LOG_MESSAGE("Match found: Exact match");
        if (outSimilarity) *outSimilarity = 1.0;
        if (outMatchType) *outMatchType = "exact";
        return searchContent;
    }
    
    double bestSim = 0.0;
    QString bestMatch = findBestMatch(fileContent, searchContent, 0.0, &bestSim);
    
    if (!bestMatch.isEmpty() && bestSim >= 0.70) {
        LOG_MESSAGE(QString("Match found: Fuzzy match (%1%% similarity)")
                       .arg(qRound(bestSim * 100)));
        if (outSimilarity) *outSimilarity = bestSim;
        if (outMatchType) {
            if (bestSim >= 0.85) {
                *outMatchType = "fuzzy_high";
            } else {
                *outMatchType = "fuzzy_medium";
            }
        }
        return bestMatch;
    }
    
    LOG_MESSAGE(QString("Cannot proceed: similarity too low (%1%%). "
                       "File may have been auto-formatted or manually edited.")
                   .arg(qRound(bestSim * 100)));
    
    if (outSimilarity) *outSimilarity = bestSim;
    if (outMatchType) *outMatchType = "none";
    
    return QString();
}

bool ChangesManager::performFragmentReplacement(
    const QString &filePath,
    const QString &searchContent,
    const QString &replaceContent,
    bool isAppendOperation,
    QString *errorMsg)
{
    QString currentContent = readFileContent(filePath);
    if (currentContent.isNull()) {
        if (errorMsg) {
            *errorMsg = "Failed to read current file content";
        }
        LOG_MESSAGE(QString("Failed to read file for fragment replacement: %1").arg(filePath));
        return false;
    }
    
    QString resultContent;
    
    if (isAppendOperation) {
        if (searchContent.isEmpty()) {
            resultContent = currentContent + replaceContent;
            if (errorMsg) *errorMsg = "Successfully applied";
        } else {
            if (currentContent.endsWith(searchContent)) {
                resultContent = currentContent.left(currentContent.length() - searchContent.length());
                if (errorMsg) *errorMsg = "Successfully undone";
            } else {
                if (errorMsg) {
                    *errorMsg = "Cannot undo: appended content not found at end of file";
                }
                LOG_MESSAGE(QString("Failed to undo append: content not at end: %1").arg(filePath));
                return false;
            }
        }
    } else {
        double similarity = 0.0;
        QString matchType;
        QString matchedContent = findBestMatchWithNormalization(
            currentContent, searchContent, &similarity, &matchType);
        
        if (!matchedContent.isEmpty()) {
            resultContent = currentContent;
            resultContent.replace(matchedContent, replaceContent);
            
            if (errorMsg) {
                if (matchType == "exact") {
                    *errorMsg = "Successfully applied";
                } else if (matchType.startsWith("fuzzy")) {
                    *errorMsg = QString("Applied (%1%% similarity)")
                                  .arg(qRound(similarity * 100));
                }
            }
        } else {
            if (errorMsg) {
                *errorMsg = QString("Cannot apply: similarity too low (%1%%). File may have been modified.")
                              .arg(qRound(similarity * 100));
            }
            LOG_MESSAGE(QString("Failed to find content for fragment replacement: %1 (similarity: %2%%)")
                           .arg(filePath).arg(qRound(similarity * 100)));
            return false;
        }
    }
    
    DiffInfo freshDiff = createDiffInfo(currentContent, resultContent, filePath);
    return performFileEditWithDiff(filePath, freshDiff, false, errorMsg);
}

bool ChangesManager::applyPendingEditsForRequest(const QString &requestId, QString *errorMsg)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_requestEdits.contains(requestId)) {
        LOG_MESSAGE(QString("No edits tracked for request: %1").arg(requestId));
        return true;
    }
    
    const RequestEdits &reqEdits = m_requestEdits[requestId];
    
    QStringList notApplied;
    for (const QString &editId : reqEdits.editIds) {
        if (m_fileEdits.contains(editId)) {
            const FileEdit &edit = m_fileEdits[editId];
            if (edit.status == Pending) {
                notApplied.append(QString("%1 (pending)").arg(edit.filePath));
            }
        }
    }
    
    if (!notApplied.isEmpty()) {
        LOG_MESSAGE(QString("Request %1 has %2 edits that were not auto-applied")
                       .arg(requestId).arg(notApplied.size()));
        if (errorMsg) {
            *errorMsg = QString("%1 edit(s) were not auto-applied:\n%2")
                           .arg(notApplied.size())
                           .arg(notApplied.join("\n"));
        }
        return false;
    }
    
    LOG_MESSAGE(QString("All edits for request %1 are applied").arg(requestId));
    return true;
}

QList<ChangesManager::FileEdit> ChangesManager::getEditsForRequest(const QString &requestId) const
{
    QMutexLocker locker(&m_mutex);
    
    QList<FileEdit> edits;
    
    if (!m_requestEdits.contains(requestId)) {
        return edits;
    }
    
    const RequestEdits &reqEdits = m_requestEdits[requestId];
    for (const QString &editId : reqEdits.editIds) {
        if (m_fileEdits.contains(editId)) {
            edits.append(m_fileEdits[editId]);
        }
    }
    
    return edits;
}

bool ChangesManager::undoAllEditsForRequest(const QString &requestId, QString *errorMsg)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_requestEdits.contains(requestId)) {
        LOG_MESSAGE(QString("No edits found for request: %1").arg(requestId));
        return true;
    }
    
    const RequestEdits &reqEdits = m_requestEdits[requestId];
    QStringList failedUndos;
    int successCount = 0;
    
    LOG_MESSAGE(QString("Undoing %1 edits for request: %2")
                    .arg(reqEdits.editIds.size()).arg(requestId));
    
    for (int i = reqEdits.editIds.size() - 1; i >= 0; --i) {
        const QString &editId = reqEdits.editIds[i];
        
        if (!m_fileEdits.contains(editId)) {
            LOG_MESSAGE(QString("Edit not found during undo: %1").arg(editId));
            continue;
        }
        
        FileEdit &edit = m_fileEdits[editId];
        
        if (edit.status == Archived) {
            LOG_MESSAGE(QString("Skipping archived edit: %1").arg(editId));
            continue;
        }
        
        if (edit.status != Applied) {
            LOG_MESSAGE(QString("Edit %1 is not applied (status: %2), skipping").arg(editId).arg(edit.status));
            continue;
        }
        
        QString filePathCopy = edit.filePath;
        QString oldContentCopy = edit.oldContent;
        QString newContentCopy = edit.newContent;
        
        locker.unlock();
        
        LOG_MESSAGE(QString("Undoing edit %1 using REVERSE fragment replacement (mass undo)").arg(editId));
        
        QString errMsg;
        bool isAppend = oldContentCopy.isEmpty();
        bool success = performFragmentReplacement(
            filePathCopy, newContentCopy, oldContentCopy, isAppend, &errMsg);
        
        locker.relock();
        
        if (success) {
            edit.status = Rejected;
            edit.statusMessage = errMsg.isEmpty() ? "Undone by mass undo" : errMsg;
            edit.wasAutoApplied = false;
            successCount++;
            
            locker.unlock();
            emit fileEditUndone(editId);
            locker.relock();
            
            LOG_MESSAGE(QString("Undone edit %1 for file: %2").arg(editId, edit.filePath));
        } else {
            edit.statusMessage = errMsg.isEmpty() ? "Failed to undo" : errMsg;
            failedUndos.append(QString("%1: %2").arg(edit.filePath, edit.statusMessage));
            
            LOG_MESSAGE(QString("Failed to undo edit %1: %2").arg(editId, edit.statusMessage));
        }
    }
    
    LOG_MESSAGE(QString("Undone %1/%2 edits for request %3")
                    .arg(successCount).arg(reqEdits.editIds.size()).arg(requestId));
    
    if (!failedUndos.isEmpty()) {
        if (errorMsg) {
            *errorMsg = QString("Failed to undo %1 edit(s):\n%2")
                           .arg(failedUndos.size())
                           .arg(failedUndos.join("\n"));
        }
        return false;
    }
    
    return true;
}

bool ChangesManager::reapplyAllEditsForRequest(const QString &requestId, QString *errorMsg)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_requestEdits.contains(requestId)) {
        LOG_MESSAGE(QString("No edits found for request: %1").arg(requestId));
        return true;
    }
    
    const RequestEdits &reqEdits = m_requestEdits[requestId];
    QStringList failedApplies;
    int successCount = 0;
    
    LOG_MESSAGE(QString("Reapplying %1 edits for request: %2")
                    .arg(reqEdits.editIds.size()).arg(requestId));
    
    for (const QString &editId : reqEdits.editIds) {
        if (!m_fileEdits.contains(editId)) {
            LOG_MESSAGE(QString("Edit not found during reapply: %1").arg(editId));
            continue;
        }
        
        FileEdit &edit = m_fileEdits[editId];
        
        if (edit.status == Archived) {
            LOG_MESSAGE(QString("Cannot reapply archived edit: %1").arg(editId));
            continue;
        }
        
        if (edit.status == Applied) {
            LOG_MESSAGE(QString("Edit %1 is already applied, skipping").arg(editId));
            successCount++;
            continue;
        }
        
        QString filePathCopy = edit.filePath;
        QString oldContentCopy = edit.oldContent;
        QString newContentCopy = edit.newContent;
        
        locker.unlock();
        
        LOG_MESSAGE(QString("Reapplying edit %1 using fragment replacement (mass apply)").arg(editId));
        
        QString errMsg;
        bool isAppend = oldContentCopy.isEmpty();
        bool success = performFragmentReplacement(
            filePathCopy, oldContentCopy, newContentCopy, isAppend, &errMsg);
        
        locker.relock();
        
        if (success) {
            edit.status = Applied;
            edit.statusMessage = errMsg.isEmpty() ? "Reapplied successfully" : errMsg;
            successCount++;
            
            locker.unlock();
            emit fileEditApplied(editId);
            locker.relock();
            
            LOG_MESSAGE(QString("Reapplied edit %1 for file: %2 (%3)")
                           .arg(editId, edit.filePath, edit.statusMessage));
        } else {
            edit.statusMessage = errMsg.isEmpty() ? "Failed to reapply" : errMsg;
            failedApplies.append(QString("%1: %2").arg(edit.filePath, edit.statusMessage));
            
            LOG_MESSAGE(QString("Failed to reapply edit %1: %2").arg(editId, edit.statusMessage));
        }
    }
    
    LOG_MESSAGE(QString("Reapplied %1/%2 edits for request %3")
                    .arg(successCount).arg(reqEdits.editIds.size()).arg(requestId));
    
    if (!failedApplies.isEmpty()) {
        if (errorMsg) {
            *errorMsg = QString("Failed to reapply %1 edit(s):\n%2")
                           .arg(failedApplies.size())
                           .arg(failedApplies.join("\n"));
        }
        return false;
    }
    
    return true;
}

void ChangesManager::archiveAllNonArchivedEdits()
{
    QMutexLocker locker(&m_mutex);
    
    QStringList archivedEdits;
    
    for (auto it = m_fileEdits.begin(); it != m_fileEdits.end(); ++it) {
        FileEdit &edit = it.value();
        
        if (edit.status != Archived) {
            FileEditStatus oldStatus = edit.status;
            
            edit.status = Archived;
            edit.statusMessage = "Archived (from previous conversation turn)";
            archivedEdits.append(edit.editId);
            
            LOG_MESSAGE(QString("Archived file edit: %1 (file: %2, was: %3)")
                           .arg(edit.editId, edit.filePath, 
                                oldStatus == Applied ? "applied" : 
                                oldStatus == Rejected ? "rejected" : "pending"));
        }
    }
    
    locker.unlock();
    for (const QString &editId : archivedEdits) {
        emit fileEditArchived(editId);
    }
    
    if (!archivedEdits.isEmpty()) {
        LOG_MESSAGE(QString("Archived %1 file edit(s) from previous conversation turn")
                       .arg(archivedEdits.size()));
    }
}

QString ChangesManager::readFileContent(const QString &filePath) const
{
    LOG_MESSAGE(QString("Reading current file content: %1").arg(filePath));
    
    auto editors = Core::EditorManager::visibleEditors();
    for (auto *editor : editors) {
        if (!editor || !editor->document()) {
            continue;
        }

        QString editorPath = editor->document()->filePath().toFSPathString();
        if (editorPath == filePath) {
            QByteArray contentBytes = editor->document()->contents();
            QString content = QString::fromUtf8(contentBytes);
            LOG_MESSAGE(QString("  Read from open editor: %1 bytes").arg(content.size()));
            return content;
        }
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("  Failed to read file: %1").arg(file.errorString()));
        return QString(); // Return null QString on error
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    LOG_MESSAGE(QString("  Read from disk: %1 bytes").arg(content.size()));
    return content;
}

bool ChangesManager::performFileEditWithDiff(
    const QString &filePath, 
    const DiffInfo &diffInfo, 
    bool reverse,
    QString *errorMsg)
{
    LOG_MESSAGE(QString("=== performFileEditWithDiff: %1 (reverse: %2) ===")
                   .arg(filePath).arg(reverse ? "yes" : "no"));
    
    auto setError = [errorMsg](const QString &msg) {
        if (errorMsg) *errorMsg = msg;
    };
    
    auto editors = Core::EditorManager::visibleEditors();
    LOG_MESSAGE(QString("  Checking %1 visible editor(s)").arg(editors.size()));
    
    for (auto *editor : editors) {
        if (!editor || !editor->document()) {
            continue;
        }

        QString editorPath = editor->document()->filePath().toFSPathString();
        if (editorPath == filePath) {
            LOG_MESSAGE(QString("  Found open editor for: %1").arg(filePath));
            
            QByteArray contentBytes = editor->document()->contents();
            QString currentContent = QString::fromUtf8(contentBytes);
            
            LOG_MESSAGE(QString("  Current content size: %1 bytes").arg(currentContent.size()));
            
            QString modifiedContent = currentContent;
            QString diffErrorMsg;
            bool diffSuccess = applyDiffToContent(modifiedContent, diffInfo, reverse, &diffErrorMsg);
            
            if (!diffSuccess) {
                LOG_MESSAGE(QString("  Failed to apply diff: %1").arg(diffErrorMsg));
                setError(diffErrorMsg);
                
                LOG_MESSAGE("  Attempting fallback to old content-based method...");
                QString oldContent = reverse ? diffInfo.modifiedContent : diffInfo.originalContent;
                QString newContent = reverse ? diffInfo.originalContent : diffInfo.modifiedContent;
                
                return performFileEdit(filePath, oldContent, newContent, errorMsg);
            }
            
            if (auto *textEditor = qobject_cast<TextEditor::TextDocument *>(editor->document())) {
                QTextDocument *doc = textEditor->document();
                
                LOG_MESSAGE("  Applying changes to text editor document...");
                
                if (!doc) {
                    LOG_MESSAGE("  Document is invalid");
                    setError("Document pointer is null");
                    return false;
                }

                
                bool oldBlockState = doc->blockSignals(true);
                
                try {
                    QTextCursor cursor(doc);
                    
                    if (cursor.isNull()) {
                        doc->blockSignals(oldBlockState);
                        LOG_MESSAGE("  Cursor is invalid");
                        setError("Cannot create text cursor");
                        return false;
                    }
                    
                    cursor.beginEditBlock();
                    cursor.select(QTextCursor::Document);
                    cursor.removeSelectedText();
                    cursor.insertText(modifiedContent);
                    cursor.endEditBlock();
                    
                    doc->blockSignals(oldBlockState);
                    
                    emit doc->contentsChange(0, doc->characterCount(), doc->characterCount());
                    
                    LOG_MESSAGE(QString("  ✓ Successfully applied diff to open editor: %1").arg(filePath));
                    setError(diffErrorMsg);
                    return true;
                } catch (...) {
                    doc->blockSignals(oldBlockState);
                    LOG_MESSAGE("  Exception during document modification");
                    setError("Exception during document modification");
                    return false;
                }

                doc->blockSignals(false);
            }
        }
    }

    LOG_MESSAGE("  File not open in editor, modifying file directly...");

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString msg = QString("Cannot open file: %1").arg(file.errorString());
        LOG_MESSAGE(QString("  Failed to open file for reading: %1 - %2")
                       .arg(filePath, file.errorString()));
        setError(msg);
        return false;
    }

    QString currentContent = QString::fromUtf8(file.readAll());
    file.close();
    
    LOG_MESSAGE(QString("  File read successfully (%1 bytes)").arg(currentContent.size()));
    
    QString modifiedContent = currentContent;
    QString diffErrorMsg;
    bool diffSuccess = applyDiffToContent(modifiedContent, diffInfo, reverse, &diffErrorMsg);
    
    if (!diffSuccess) {
        LOG_MESSAGE(QString("  Failed to apply diff to file: %1").arg(diffErrorMsg));
        setError(diffErrorMsg);
        
        LOG_MESSAGE("  Attempting fallback to old content-based method...");
        QString oldContent = reverse ? diffInfo.modifiedContent : diffInfo.originalContent;
        QString newContent = reverse ? diffInfo.originalContent : diffInfo.modifiedContent;
        
        return performFileEdit(filePath, oldContent, newContent, errorMsg);
    }
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QString msg = QString("Cannot write file: %1").arg(file.errorString());
        LOG_MESSAGE(QString("  Failed to open file for writing: %1 - %2")
                       .arg(filePath, file.errorString()));
        setError(msg);
        return false;
    }

    QTextStream out(&file);
    out << modifiedContent;
    file.close();
    
    LOG_MESSAGE(QString("  ✓ Successfully wrote modified content to file: %1").arg(filePath));
    setError(diffErrorMsg);
    return true;
}

ChangesManager::DiffInfo ChangesManager::createDiffInfo(
    const QString &originalContent, 
    const QString &modifiedContent,
    const QString &filePath)
{
    LOG_MESSAGE(QString("=== Creating DiffInfo for file: %1 ===").arg(filePath));
    
    DiffInfo diffInfo;
    diffInfo.originalContent = originalContent;
    diffInfo.modifiedContent = modifiedContent;
    diffInfo.contextLines = 3;
    
    QStringList originalLines = originalContent.split('\n');
    QStringList modifiedLines = modifiedContent.split('\n');
    
    LOG_MESSAGE(QString("  Original lines: %1, Modified lines: %2")
                   .arg(originalLines.size()).arg(modifiedLines.size()));
    
    int origLen = originalLines.size();
    int modLen = modifiedLines.size();
    
    QVector<QVector<int>> lcs(origLen + 1, QVector<int>(modLen + 1, 0));
    
    for (int i = 1; i <= origLen; ++i) {
        for (int j = 1; j <= modLen; ++j) {
            if (originalLines[i - 1] == modifiedLines[j - 1]) {
                lcs[i][j] = lcs[i - 1][j - 1] + 1;
            } else {
                lcs[i][j] = qMax(lcs[i - 1][j], lcs[i][j - 1]);
            }
        }
    }
    
    LOG_MESSAGE(QString("  LCS computation complete. LCS length: %1").arg(lcs[origLen][modLen]));
    
    QList<DiffHunk> hunks;
    DiffHunk currentHunk;
    bool inHunk = false;
    
    int i = origLen;
    int j = modLen;
    int hunkCount = 0;
    
    QList<QPair<int, int>> changes;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && originalLines[i - 1] == modifiedLines[j - 1]) {
            changes.prepend({i - 1, j - 1});
            i--;
            j--;
        } else if (j > 0 && (i == 0 || lcs[i][j - 1] >= lcs[i - 1][j])) {
            changes.prepend({-1, j - 1});
            j--;
        } else if (i > 0) {
            changes.prepend({i - 1, -1});
            i--;
        }
    }
    
    LOG_MESSAGE(QString("  Backtracking complete. Total operations: %1").arg(changes.size()));
    
    int idx = 0;
    while (idx < changes.size()) {
        while (idx < changes.size() && changes[idx].first != -1 && changes[idx].second != -1) {
            idx++;
        }
        
        if (idx >= changes.size()) break;
        
        DiffHunk hunk;
        int hunkStartIdx = idx;
        
        int contextStart = qMax(0, idx - diffInfo.contextLines);
        for (int k = contextStart; k < idx; ++k) {
            if (changes[k].first != -1) {
                hunk.contextBefore.append(originalLines[changes[k].first]);
            }
        }
        
        hunk.oldStartLine = changes[hunkStartIdx].first != -1 
                           ? changes[hunkStartIdx].first + 1 
                           : (hunkStartIdx > 0 && changes[hunkStartIdx - 1].first != -1 
                              ? changes[hunkStartIdx - 1].first + 2 
                              : 1);
        hunk.newStartLine = changes[hunkStartIdx].second != -1 
                           ? changes[hunkStartIdx].second + 1 
                           : (hunkStartIdx > 0 && changes[hunkStartIdx - 1].second != -1 
                              ? changes[hunkStartIdx - 1].second + 2 
                              : 1);
        
        int lastChangeIdx = idx;
        while (idx < changes.size()) {
            if (changes[idx].first == -1) {
                hunk.addedLines.append(modifiedLines[changes[idx].second]);
                lastChangeIdx = idx;
            } else if (changes[idx].second == -1) {
                hunk.removedLines.append(originalLines[changes[idx].first]);
                lastChangeIdx = idx;
            } else {
                if (idx - lastChangeIdx > diffInfo.contextLines * 2) {
                    break;
                }
            }
            idx++;
        }
        
        hunk.oldLineCount = hunk.removedLines.size();
        hunk.newLineCount = hunk.addedLines.size();
        
        int contextEnd = qMin(changes.size(), idx + diffInfo.contextLines);
        for (int k = idx; k < contextEnd; ++k) {
            if (changes[k].first != -1) {
                hunk.contextAfter.append(originalLines[changes[k].first]);
            }
        }
        
        hunks.append(hunk);
        hunkCount++;
        
        LOG_MESSAGE(QString("  Hunk #%1: oldStart=%2, oldCount=%3, newStart=%4, newCount=%5")
                       .arg(hunkCount)
                       .arg(hunk.oldStartLine)
                       .arg(hunk.oldLineCount)
                       .arg(hunk.newStartLine)
                       .arg(hunk.newLineCount));
        LOG_MESSAGE(QString("    Context before: %1 lines, Context after: %2 lines")
                       .arg(hunk.contextBefore.size())
                       .arg(hunk.contextAfter.size()));
        LOG_MESSAGE(QString("    Removed: %1 lines, Added: %2 lines")
                       .arg(hunk.removedLines.size())
                       .arg(hunk.addedLines.size()));
    }
    
    diffInfo.hunks = hunks;
    
    if (hunks.isEmpty() && originalContent != modifiedContent) {
        LOG_MESSAGE("  WARNING: No hunks created but content differs. Using fallback mode.");
        diffInfo.useFallback = true;
    } else if (hunks.isEmpty()) {
        LOG_MESSAGE("  No changes detected (content identical).");
    } else {
        LOG_MESSAGE(QString("=== DiffInfo created successfully with %1 hunk(s) ===").arg(hunks.size()));
    }
    
    return diffInfo;
}

bool ChangesManager::findHunkLocation(
    const QStringList &fileLines, 
    const DiffHunk &hunk, 
    int &actualStartLine,
    QString *debugInfo) const
{
    LOG_MESSAGE(QString("  Searching for hunk location (expected line: %1)").arg(hunk.oldStartLine));
    
    QString debug;
    
    int expectedIdx = hunk.oldStartLine - 1;
    
    if (expectedIdx >= 0 && expectedIdx < fileLines.size()) {
        bool exactMatch = true;
        
        int checkIdx = expectedIdx - hunk.contextBefore.size();
        if (checkIdx < 0) {
            exactMatch = false;
            debug += QString("    Context before out of bounds (need %1 lines before line %2)\n")
                        .arg(hunk.contextBefore.size()).arg(expectedIdx + 1);
        } else {
            for (int i = 0; i < hunk.contextBefore.size(); ++i) {
                if (fileLines[checkIdx + i] != hunk.contextBefore[i]) {
                    exactMatch = false;
                    debug += QString("    Context before mismatch at offset %1: expected '%2', got '%3'\n")
                                .arg(i).arg(hunk.contextBefore[i]).arg(fileLines[checkIdx + i]);
                    break;
                }
            }
        }
        
        if (exactMatch) {
            for (int i = 0; i < hunk.removedLines.size(); ++i) {
                int lineIdx = expectedIdx + i;
                if (lineIdx >= fileLines.size() || fileLines[lineIdx] != hunk.removedLines[i]) {
                    exactMatch = false;
                    debug += QString("    Removed line mismatch at offset %1: expected '%2', got '%3'\n")
                                .arg(i)
                                .arg(hunk.removedLines[i])
                                .arg(lineIdx < fileLines.size() ? fileLines[lineIdx] : "<EOF>");
                    break;
                }
            }
        }
        
        if (exactMatch && !hunk.contextAfter.isEmpty()) {
            int afterIdx = expectedIdx + hunk.removedLines.size();
            for (int i = 0; i < hunk.contextAfter.size(); ++i) {
                int lineIdx = afterIdx + i;
                if (lineIdx >= fileLines.size() || fileLines[lineIdx] != hunk.contextAfter[i]) {
                    exactMatch = false;
                    debug += QString("    Context after mismatch at offset %1: expected '%2', got '%3'\n")
                                .arg(i)
                                .arg(hunk.contextAfter[i])
                                .arg(lineIdx < fileLines.size() ? fileLines[lineIdx] : "<EOF>");
                    break;
                }
            }
        }
        
        if (exactMatch) {
            actualStartLine = expectedIdx;
            LOG_MESSAGE(QString("  ✓ Found exact match at expected line %1").arg(hunk.oldStartLine));
            if (debugInfo) *debugInfo = "Exact match at expected location";
            return true;
        } else {
            debug += "  Exact match at expected location failed, trying fuzzy search...\n";
        }
    } else {
        debug += QString("  Expected location %1 is out of bounds (file has %2 lines)\n")
                    .arg(hunk.oldStartLine).arg(fileLines.size());
    }
    
    LOG_MESSAGE("  Trying fuzzy search within ±20 lines...");
    
    int searchStart = qMax(0, expectedIdx - 20);
    int searchEnd = qMin(fileLines.size(), expectedIdx + 20);
    
    int bestMatchLine = -1;
    int bestMatchScore = 0;
    
    for (int searchIdx = searchStart; searchIdx < searchEnd; ++searchIdx) {
        int matchScore = 0;
        int totalChecks = 0;
        
        int checkIdx = searchIdx - hunk.contextBefore.size();
        if (checkIdx >= 0) {
            for (int i = 0; i < hunk.contextBefore.size(); ++i) {
                totalChecks++;
                if (fileLines[checkIdx + i] == hunk.contextBefore[i]) {
                    matchScore++;
                }
            }
        }
        
        for (int i = 0; i < hunk.removedLines.size(); ++i) {
            int lineIdx = searchIdx + i;
            if (lineIdx < fileLines.size()) {
                totalChecks++;
                if (fileLines[lineIdx] == hunk.removedLines[i]) {
                    matchScore++;
                }
            }
        }
        
        int afterIdx = searchIdx + hunk.removedLines.size();
        for (int i = 0; i < hunk.contextAfter.size(); ++i) {
            int lineIdx = afterIdx + i;
            if (lineIdx < fileLines.size()) {
                totalChecks++;
                if (fileLines[lineIdx] == hunk.contextAfter[i]) {
                    matchScore++;
                }
            }
        }
        
        if (matchScore > bestMatchScore) {
            bestMatchScore = matchScore;
            bestMatchLine = searchIdx;
        }
    }
    
    int totalPossibleScore = hunk.contextBefore.size() + hunk.removedLines.size() + hunk.contextAfter.size();
    double matchPercentage = totalPossibleScore > 0 ? (double)bestMatchScore / totalPossibleScore * 100.0 : 0.0;
    
    if (bestMatchLine != -1 && matchPercentage >= 70.0) {
        actualStartLine = bestMatchLine;
        debug += QString("  ✓ Found fuzzy match at line %1 (score: %2/%3 = %4%%)\n")
                    .arg(bestMatchLine + 1)
                    .arg(bestMatchScore)
                    .arg(totalPossibleScore)
                    .arg(matchPercentage, 0, 'f', 1);
        LOG_MESSAGE(QString("  ✓ Found fuzzy match at line %1 (%2%% confidence)")
                       .arg(bestMatchLine + 1).arg(matchPercentage, 0, 'f', 1));
        if (debugInfo) *debugInfo = debug;
        return true;
    }
    
    debug += QString("  ✗ No suitable match found (best: %1%% at line %2)\n")
                .arg(matchPercentage, 0, 'f', 1)
                .arg(bestMatchLine != -1 ? bestMatchLine + 1 : -1);
    LOG_MESSAGE(QString("  ✗ Hunk location not found (best match: %1%%)").arg(matchPercentage, 0, 'f', 1));
    
    if (debugInfo) *debugInfo = debug;
    return false;
}

bool ChangesManager::applyDiffToContent(
    QString &content, 
    const DiffInfo &diffInfo, 
    bool reverse,
    QString *errorMsg)
{
    LOG_MESSAGE(QString("=== Applying %1 to content ===").arg(reverse ? "REVERSE diff" : "diff"));
    
    auto setError = [errorMsg](const QString &msg) {
        if (errorMsg) *errorMsg = msg;
    };
    
    if (diffInfo.useFallback) {
        LOG_MESSAGE("  Using fallback mode (direct content replacement)");
        
        QString searchContent = reverse ? diffInfo.modifiedContent : diffInfo.originalContent;
        QString replaceContent = reverse ? diffInfo.originalContent : diffInfo.modifiedContent;
        
        if (content.contains(searchContent)) {
            content.replace(searchContent, replaceContent);
            setError("Applied using fallback mode (direct replacement)");
            LOG_MESSAGE("  ✓ Fallback: Direct replacement successful");
            return true;
        } else {
            setError("Fallback failed: Original content not found in file");
            LOG_MESSAGE("  ✗ Fallback: Content not found");
            return false;
        }
    }
    
    if (diffInfo.hunks.isEmpty()) {
        LOG_MESSAGE("  No hunks to apply (content unchanged)");
        setError("No changes to apply");
        return true;
    }
    
    QStringList fileLines = content.split('\n');
    LOG_MESSAGE(QString("  File has %1 lines, applying %2 hunk(s)")
                   .arg(fileLines.size()).arg(diffInfo.hunks.size()));
    
    QList<DiffHunk> hunksToApply = diffInfo.hunks;
    
    std::sort(hunksToApply.begin(), hunksToApply.end(), 
              [](const DiffHunk &a, const DiffHunk &b) {
                  return a.oldStartLine > b.oldStartLine;
              });
    
    LOG_MESSAGE("  Hunks sorted in descending order for application");
    
    int appliedHunks = 0;
    int failedHunks = 0;
    
    for (int hunkIdx = 0; hunkIdx < hunksToApply.size(); ++hunkIdx) {
        const DiffHunk &hunk = hunksToApply[hunkIdx];
        
        LOG_MESSAGE(QString("  --- Applying hunk %1/%2 ---")
                       .arg(hunkIdx + 1).arg(hunksToApply.size()));
        
        int actualStartLine = -1;
        QString debugInfo;
        
        if (!findHunkLocation(fileLines, hunk, actualStartLine, &debugInfo)) {
            LOG_MESSAGE(QString("  ✗ Failed to locate hunk %1:\n%2")
                           .arg(hunkIdx + 1).arg(debugInfo));
            failedHunks++;
            continue;
        }
        
        LOG_MESSAGE(QString("  Applying hunk at line %1 (remove %2 lines, add %3 lines)")
                       .arg(actualStartLine + 1)
                       .arg(hunk.removedLines.size())
                       .arg(hunk.addedLines.size()));
        
        for (int i = 0; i < hunk.removedLines.size(); ++i) {
            if (actualStartLine < fileLines.size()) {
                LOG_MESSAGE(QString("    Removing line %1: '%2'")
                               .arg(actualStartLine + 1)
                               .arg(fileLines[actualStartLine]));
                fileLines.removeAt(actualStartLine);
            }
        }
        
        for (int i = 0; i < hunk.addedLines.size(); ++i) {
            LOG_MESSAGE(QString("    Inserting line %1: '%2'")
                           .arg(actualStartLine + i + 1)
                           .arg(hunk.addedLines[i]));
            fileLines.insert(actualStartLine + i, hunk.addedLines[i]);
        }
        
        appliedHunks++;
        LOG_MESSAGE(QString("  ✓ Hunk %1 applied successfully").arg(hunkIdx + 1));
    }
    
    if (failedHunks > 0) {
        QString msg = QString("Partially applied: %1 of %2 hunks succeeded")
                         .arg(appliedHunks).arg(hunksToApply.size());
        setError(msg);
        LOG_MESSAGE(QString("  ⚠ %1").arg(msg));
        
        content = fileLines.join('\n');
        return false;
    }
    
    content = fileLines.join('\n');
    setError(QString("Successfully applied %1 hunk(s)").arg(appliedHunks));
    LOG_MESSAGE(QString("=== All %1 hunk(s) applied successfully ===").arg(appliedHunks));
    return true;
}

} // namespace QodeAssist::Context
