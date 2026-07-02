// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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

QString ChangesManager::getRecentChangesContext(const QString &currentFilePath) const
{
    QString context;
    for (auto it = m_documentChanges.constBegin(); it != m_documentChanges.constEnd(); ++it) {
        if (it.key() && it.key()->filePath().toFSPathString() == currentFilePath)
            continue;
        for (const auto &change : it.value()) {
            context += change.lineContent + "\n";
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
        filePathCopy, newContentCopy, oldContentCopy, isAppend, &errorMsg, true);
    
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

int ChangesManager::levenshteinDistance(const QString &s1, const QString &s2) const
{
    const int len1 = s1.length();
    const int len2 = s2.length();
    
    const int MAX_LENGTH = 10000;
    if (len1 > MAX_LENGTH || len2 > MAX_LENGTH) {
        return qAbs(len1 - len2) + qMin(len1, len2) / 2;
    }
    
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

QString ChangesManager::findBestMatchLineBased(
    const QString &fileContent,
    const QString &searchContent,
    double threshold,
    double *outSimilarity) const
{
    QStringList fileLines = fileContent.split('\n');
    QStringList searchLines = searchContent.split('\n');
    
    if (searchLines.isEmpty() || fileLines.isEmpty()) {
        if (outSimilarity) *outSimilarity = 0.0;
        return QString();
    }
    
    if (searchLines.size() > fileLines.size()) {
        if (outSimilarity) *outSimilarity = 0.0;
        return QString();
    }
    
    QString bestMatch;
    double bestSimilarity = 0.0;
    int searchLineCount = searchLines.size();
    
    LOG_MESSAGE(QString("Line-based search: %1 search lines in %2 file lines")
                   .arg(searchLineCount).arg(fileLines.size()));
    
    for (int i = 0; i <= fileLines.size() - searchLineCount; ++i) {
        int matchingLines = 0;
        int totalLines = searchLineCount;
        
        for (int j = 0; j < searchLineCount; ++j) {
            if (fileLines[i + j] == searchLines[j]) {
                matchingLines++;
            }
        }
        
        double similarity = static_cast<double>(matchingLines) / totalLines;
        
        if (similarity > bestSimilarity) {
            bestSimilarity = similarity;
            if (similarity >= threshold) {
                QStringList matchedLines;
                for (int j = 0; j < searchLineCount; ++j) {
                    matchedLines.append(fileLines[i + j]);
                }
                bestMatch = matchedLines.join('\n');
                
                if (similarity >= 0.99) {
                    if (outSimilarity) *outSimilarity = similarity;
                    LOG_MESSAGE(QString("Found exact line match at line %1").arg(i + 1));
                    return bestMatch;
                }
            }
        }
    }
    
    if (outSimilarity) {
        *outSimilarity = bestSimilarity;
    }
    
    LOG_MESSAGE(QString("Line-based search complete, best similarity: %1%%")
                   .arg(qRound(bestSimilarity * 100)));
    
    return bestMatch;
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
    
    const int MAX_SEARCH_LENGTH = 50000;
    if (searchLen > MAX_SEARCH_LENGTH) {
        LOG_MESSAGE(QString("Search content too large (%1 chars), using line-based search").arg(searchLen));
        return findBestMatchLineBased(fileContent, searchContent, threshold, outSimilarity);
    }
    
    QString bestMatch;
    double bestSimilarity = 0.0;
    
    QChar firstChar = searchContent.at(0);
    
    int step = 1;
    if (fileLen > 100000 && searchLen > 1000) {
        step = searchLen / 10;
        if (step < 1) step = 1;
    }
    
    int searchEnd = fileLen - searchLen + 1;
    
    for (int i = 0; i < searchEnd; i += step) {
        if (step == 1 && fileContent.at(i) != firstChar) {
            continue;
        }
        
        QString candidate = fileContent.mid(i, searchLen);
        
        int lengthDiff = qAbs(candidate.length() - searchLen);
        if (lengthDiff > searchLen * 0.3) {
            continue;
        }
        
        int distance = levenshteinDistance(candidate, searchContent);
        double similarity = 1.0 - (static_cast<double>(distance) / searchLen);
        
        if (similarity > bestSimilarity) {
            bestSimilarity = similarity;
            if (similarity >= threshold) {
                bestMatch = candidate;
                
                if (similarity >= 0.95) {
                    if (outSimilarity) *outSimilarity = bestSimilarity;
                    LOG_MESSAGE(QString("Found excellent match early (similarity: %1%%), stopping search").arg(qRound(similarity * 100)));
                    return bestMatch;
                }
            }
        }
        
        if (i > searchLen * 3 && bestSimilarity < 0.5) {
            LOG_MESSAGE("Early termination: no good matches found in first 3x search area");
            break;
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
    QString *errorMsg,
    bool isUndo)
{
    QString currentContent = readFileContent(filePath);
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
        double minThreshold = isUndo ? 0.70 : 0.85;
        
        LOG_MESSAGE(QString("Fragment replacement: isUndo=%1, threshold=%2%%")
                       .arg(isUndo ? "yes" : "no")
                       .arg(qRound(minThreshold * 100)));
        
        double similarity = 0.0;
        QString matchType;
        QString matchedContent = findBestMatchWithNormalization(
            currentContent, searchContent, &similarity, &matchType);
        
        if (!matchedContent.isEmpty() && similarity < minThreshold) {
            QString msg = QString("Cannot %1: similarity too low (%2%%, threshold: %3%%). %4")
                            .arg(isUndo ? "undo" : "apply")
                            .arg(qRound(similarity * 100))
                            .arg(qRound(minThreshold * 100))
                            .arg(isUndo ? "File may have been modified." 
                                        : "LLM may have provided incorrect oldContent.");
            if (errorMsg) *errorMsg = msg;
            LOG_MESSAGE(QString("Fragment replacement failed: %1").arg(msg));
            return false;
        }
        
        if (!matchedContent.isEmpty()) {
            int matchPos = currentContent.indexOf(matchedContent);
            if (matchPos == -1) {
                if (errorMsg) {
                    *errorMsg = "Internal error: matched content not found in file";
                }
                LOG_MESSAGE(QString("Internal error: matched content disappeared: %1").arg(filePath));
                return false;
            }
            
            resultContent = currentContent.left(matchPos) 
                          + replaceContent 
                          + currentContent.mid(matchPos + matchedContent.length());
            
            LOG_MESSAGE(QString("Replaced content at position %1 (length: %2 -> %3)")
                           .arg(matchPos)
                           .arg(matchedContent.length())
                           .arg(replaceContent.length()));
            
            if (errorMsg) {
                if (matchType == "exact") {
                    *errorMsg = isUndo ? "Successfully undone" : "Successfully applied";
                } else if (matchType.startsWith("fuzzy")) {
                    *errorMsg = QString("%1 (%2%% similarity)")
                                  .arg(isUndo ? "Undone" : "Applied")
                                  .arg(qRound(similarity * 100));
                }
            }
        } else {
            if (errorMsg) {
                *errorMsg = QString("Cannot %1: content not found in file.")
                              .arg(isUndo ? "undo" : "apply");
            }
            LOG_MESSAGE(QString("Failed to find content for fragment replacement: %1").arg(filePath));
            return false;
        }
    }
    
    auto editors = Core::EditorManager::visibleEditors();
    for (auto *editor : editors) {
        if (!editor || !editor->document()) {
            continue;
        }

        QString editorPath = editor->document()->filePath().toFSPathString();
        if (editorPath == filePath) {
            if (auto *textEditor = qobject_cast<TextEditor::TextDocument *>(editor->document())) {
                QTextDocument *doc = textEditor->document();
                
                try {
                    QTextCursor cursor(doc);
                    if (!cursor.isNull()) {
                        cursor.beginEditBlock();
                        cursor.select(QTextCursor::Document);
                        cursor.removeSelectedText();
                        cursor.insertText(resultContent);
                        cursor.endEditBlock();
                        
                        if (errorMsg && errorMsg->isEmpty()) {
                            *errorMsg = isUndo ? "Successfully undone" : "Successfully applied";
                        }
                        LOG_MESSAGE(QString("Applied fragment replacement to open editor: %1").arg(filePath));
                        return true;
                    }
                } catch (...) {
                    LOG_MESSAGE("Exception during document modification");
                    if (errorMsg) *errorMsg = "Exception during document modification";
                    return false;
                }
            }
        }
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QString msg = QString("Cannot write file: %1").arg(file.errorString());
        LOG_MESSAGE(QString("Failed to open file for writing: %1 - %2")
                       .arg(filePath, file.errorString()));
        if (errorMsg) *errorMsg = msg;
        return false;
    }

    QTextStream out(&file);
    out << resultContent;
    file.close();
    
    if (errorMsg && errorMsg->isEmpty()) {
        *errorMsg = isUndo ? "Successfully undone" : "Successfully applied";
    }
    LOG_MESSAGE(QString("Applied fragment replacement to file: %1").arg(filePath));
    return true;
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
            filePathCopy, newContentCopy, oldContentCopy, isAppend, &errMsg, true);
        
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
        return QString();
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    LOG_MESSAGE(QString("  Read from disk: %1 bytes").arg(content.size()));
    return content;
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

} // namespace QodeAssist::Context
