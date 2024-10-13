/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include <QTextDocument>
#include <texteditor/textdocument.h>

#include <llmcore/ContextData.hpp>

namespace QodeAssist {

struct CopyrightInfo
{
    int startLine;
    int endLine;
    bool found;
};

class DocumentContextReader
{
public:
    DocumentContextReader(TextEditor::TextDocument *textDocument);

    QString getLineText(int lineNumber, int cursorPosition = -1) const;
    QString getContextBefore(int lineNumber, int cursorPosition, int linesCount) const;
    QString getContextAfter(int lineNumber, int cursorPosition, int linesCount) const;
    QString readWholeFileBefore(int lineNumber, int cursorPosition) const;
    QString readWholeFileAfter(int lineNumber, int cursorPosition) const;
    QString getLanguageAndFileInfo() const;
    CopyrightInfo findCopyright();
    QString getContextBetween(int startLine, int endLine, int cursorPosition) const;

    CopyrightInfo copyrightInfo() const;

    LLMCore::ContextData prepareContext(int lineNumber, int cursorPosition) const;

private:
    QString getContextBefore(int lineNumber, int cursorPosition) const;
    QString getContextAfter(int lineNumber, int cursorPosition) const;
    QString getInstructions() const;

private:
    TextEditor::TextDocument *m_textDocument;
    QTextDocument *m_document;
    CopyrightInfo m_copyrightInfo;
};

} // namespace QodeAssist
