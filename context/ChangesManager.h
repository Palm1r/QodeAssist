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

#pragma once

#include <texteditor/textdocument.h>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QQueue>
#include <QTimer>
#include <QUndoStack>

namespace QodeAssist::Context {

class ChangesManager : public QObject
{
    Q_OBJECT

public:
    struct ChangeInfo
    {
        QString fileName;
        int lineNumber;
        QString lineContent;
    };

    enum FileEditStatus { Pending, Applied, Rejected, Archived };

    struct DiffHunk
    {
        int oldStartLine;       // Starting line in old file (1-based)
        int oldLineCount;       // Number of lines in old file
        int newStartLine;       // Starting line in new file (1-based)
        int newLineCount;       // Number of lines in new file
        QStringList contextBefore;  // Lines of context before the change (for anchoring)
        QStringList removedLines;   // Lines to remove (prefixed with -)
        QStringList addedLines;     // Lines to add (prefixed with +)
        QStringList contextAfter;   // Lines of context after the change (for anchoring)
    };

    struct DiffInfo
    {
        QList<DiffHunk> hunks;       // List of diff hunks
        QString originalContent;     // Full original file content (for fallback)
        QString modifiedContent;     // Full modified file content (for fallback)
        int contextLines = 3;        // Number of context lines to keep
        bool useFallback = false;    // If true, use original content-based approach
    };

    struct FileEdit
    {
        QString editId;
        QString filePath;
        QString oldContent;      // Kept for backward compatibility and fallback
        QString newContent;      // Kept for backward compatibility and fallback
        DiffInfo diffInfo;       // Initial diff (created once, may become stale after formatting)
        FileEditStatus status;
        QDateTime timestamp;
        bool wasAutoApplied = false;  // Track if edit was already auto-applied once
        bool isFromHistory = false;   // Track if edit was loaded from chat history
        QString statusMessage;
    };

    static ChangesManager &instance();

    void addChange(
        TextEditor::TextDocument *document, int position, int charsRemoved, int charsAdded);
    QString getRecentChangesContext(const TextEditor::TextDocument *currentDocument) const;

    void addFileEdit(
        const QString &editId,
        const QString &filePath,
        const QString &oldContent,
        const QString &newContent,
        bool autoApply = true,
        bool isFromHistory = false,
        const QString &requestId = QString());
    bool applyFileEdit(const QString &editId);
    bool rejectFileEdit(const QString &editId);
    bool undoFileEdit(const QString &editId);
    FileEdit getFileEdit(const QString &editId) const;
    QList<FileEdit> getPendingEdits() const;
    
    bool applyPendingEditsForRequest(const QString &requestId, QString *errorMsg = nullptr);
    
    QList<FileEdit> getEditsForRequest(const QString &requestId) const;
    
    bool undoAllEditsForRequest(const QString &requestId, QString *errorMsg = nullptr);
    
    bool reapplyAllEditsForRequest(const QString &requestId, QString *errorMsg = nullptr);
    
    void archiveAllNonArchivedEdits();

signals:
    void fileEditAdded(const QString &editId);
    void fileEditApplied(const QString &editId);
    void fileEditRejected(const QString &editId);
    void fileEditUndone(const QString &editId);
    void fileEditArchived(const QString &editId);

private:
    ChangesManager();
    ~ChangesManager();
    ChangesManager(const ChangesManager &) = delete;
    ChangesManager &operator=(const ChangesManager &) = delete;

    bool performFileEdit(const QString &filePath, const QString &oldContent, const QString &newContent, QString *errorMsg = nullptr);
    bool performFileEditWithDiff(const QString &filePath, const DiffInfo &diffInfo, bool reverse, QString *errorMsg = nullptr);
    QString readFileContent(const QString &filePath) const;
    
    DiffInfo createDiffInfo(const QString &originalContent, const QString &modifiedContent, const QString &filePath);
    bool applyDiffToContent(QString &content, const DiffInfo &diffInfo, bool reverse, QString *errorMsg = nullptr);
    bool findHunkLocation(const QStringList &fileLines, const DiffHunk &hunk, int &actualStartLine, QString *debugInfo = nullptr) const;
    
    // Helper method for fragment-based apply/undo operations
    bool performFragmentReplacement(
        const QString &filePath,
        const QString &searchContent,
        const QString &replaceContent,
        bool isAppendOperation,
        QString *errorMsg = nullptr);
    
    int levenshteinDistance(const QString &s1, const QString &s2) const;
    QString findBestMatch(const QString &fileContent, const QString &searchContent, double threshold = 0.8, double *outSimilarity = nullptr) const;
    QString findBestMatchWithNormalization(const QString &fileContent, const QString &searchContent, double *outSimilarity = nullptr, QString *outMatchType = nullptr) const;

    struct RequestEdits
    {
        QStringList editIds;
        bool autoApplyPending = false;
    };

    QHash<TextEditor::TextDocument *, QQueue<ChangeInfo>> m_documentChanges;
    QHash<QString, FileEdit> m_fileEdits;
    QHash<QString, RequestEdits> m_requestEdits;  // requestId → ordered edits
    QUndoStack *m_undoStack;
    mutable QMutex m_mutex;
};

} // namespace QodeAssist::Context
