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

#pragma once

#include <QString>
#include <QStringList>
#include <QTextBlock>
#include <QTextDocument>

#include <texteditor/texteditor.h>
#include <utils/textutils.h>

namespace QodeAssist {

class ContextExtractor
{
public:
    static QString extractBefore(TextEditor::TextEditorWidget *editor,
                                  const Utils::Text::Range &range,
                                  int lineCount)
    {
        if (!editor || lineCount <= 0) {
            return QString();
        }

        QTextDocument *doc = editor->document();
        int startLine = range.begin.line;
        int contextStartLine = qMax(1, startLine - lineCount);

        QStringList contextLines;
        for (int line = contextStartLine; line < startLine; ++line) {
            QTextBlock block = doc->findBlockByNumber(line - 1);
            if (block.isValid()) {
                contextLines.append(block.text());
            }
        }

        return contextLines.join('\n');
    }

    static QString extractAfter(TextEditor::TextEditorWidget *editor,
                                const Utils::Text::Range &range,
                                int lineCount)
    {
        if (!editor || lineCount <= 0) {
            return QString();
        }

        QTextDocument *doc = editor->document();
        int endLine = range.end.line;
        int totalLines = doc->blockCount();
        int contextEndLine = qMin(totalLines, endLine + lineCount);

        QStringList contextLines;
        for (int line = endLine + 1; line <= contextEndLine; ++line) {
            QTextBlock block = doc->findBlockByNumber(line - 1);
            if (block.isValid()) {
                contextLines.append(block.text());
            }
        }

        return contextLines.join('\n');
    }

    static QString extractLineContext(QTextDocument *doc, int position, bool before)
    {
        QTextCursor cursor(doc);
        cursor.setPosition(position);

        if (before) {
            int posInBlock = cursor.positionInBlock();
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, posInBlock);
            return cursor.selectedText();
        } else {
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            return cursor.selectedText();
        }
    }

    static QStringList extractSurroundingLines(QTextDocument *doc, int position, int linesBefore, int linesAfter)
    {
        QTextCursor cursor(doc);
        cursor.setPosition(position);
        QTextBlock currentBlock = cursor.block();

        QStringList result;

        QTextBlock blockBefore = currentBlock.previous();
        QStringList beforeLines;
        for (int i = 0; i < linesBefore && blockBefore.isValid(); ++i) {
            beforeLines.prepend(blockBefore.text());
            blockBefore = blockBefore.previous();
        }
        result.append(beforeLines);

        QTextBlock blockAfter = currentBlock.next();
        for (int i = 0; i < linesAfter && blockAfter.isValid(); ++i) {
            result.append(blockAfter.text());
            blockAfter = blockAfter.next();
        }

        return result;
    }
};

} // namespace QodeAssist

