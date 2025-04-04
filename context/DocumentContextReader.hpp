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
#include <QTextDocument>

#include <llmcore/ContextData.hpp>
#include <settings/CodeCompletionSettings.hpp>

namespace QodeAssist::Context {

struct CopyrightInfo
{
    int startLine;
    int endLine;
    bool found;
};

class DocumentContextReader
{
public:
    DocumentContextReader(
        QTextDocument *m_document, const QString &mimeType, const QString &filePath);

    QString getLineText(int lineNumber, int cursorPosition = -1) const;

    /**
     * @brief Retrieves @c linesCount lines of context ending at @c lineNumber at
     * @c cursorPosition in that line. The line at @c lineNumber is inclusive regardless of
     * @c cursorPosition.
     */
    QString getContextBefore(int lineNumber, int cursorPosition, int linesCount) const;

    /**
     * @brief Retrieves @c linesCount lines of context starting at @c lineNumber at
     * @c cursorPosition in that line. The line at @c lineNumber is inclusive regardless of
     * @c cursorPosition.
     */
    QString getContextAfter(int lineNumber, int cursorPosition, int linesCount) const;

    /**
     * @brief Retrieves whole file ending at @c lineNumber at @c cursorPosition in that line.
     */
    QString readWholeFileBefore(int lineNumber, int cursorPosition) const;

    /**
     * @brief Retrieves whole file starting at @c lineNumber at @c cursorPosition in that line.
     */
    QString readWholeFileAfter(int lineNumber, int cursorPosition) const;

    QString getLanguageAndFileInfo() const;
    CopyrightInfo findCopyright();
    QString getContextBetween(
        int startLine, int startCursorPosition, int endLine, int endCursorPosition) const;

    CopyrightInfo copyrightInfo() const;

    LLMCore::ContextData prepareContext(
        int lineNumber, int cursorPosition, const Settings::CodeCompletionSettings &settings) const;

private:
    TextEditor::TextDocument *m_textDocument;
    QTextDocument *m_document;
    QString m_mimeType;
    QString m_filePath;

    // Used to omit copyright headers from context. If context would otherwise include copyright
    // header it is excluded by deleting it from the returned context. This means, that the
    // returned context may contain less information than requested. If the cursor is within copyright
    // header, then the context may be empty if the context window is small.
    CopyrightInfo m_copyrightInfo;
};

} // namespace QodeAssist::Context
