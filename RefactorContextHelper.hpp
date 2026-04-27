// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QTextCursor>
#include <QTextBlock>

#include <texteditor/texteditor.h>
#include <utils/textutils.h>

namespace QodeAssist {

struct RefactorContext
{
    QString originalText;
    QString textBeforeCursor;
    QString textAfterCursor;
    QString contextBefore;
    QString contextAfter;
    int startPos{0};
    int endPos{0};
    bool isInsertion{false};
};

class RefactorContextHelper
{
public:
    static RefactorContext extractContext(TextEditor::TextEditorWidget *editor, 
                                          const Utils::Text::Range &range,
                                          int contextLinesBefore = 3,
                                          int contextLinesAfter = 3)
    {
        RefactorContext ctx;
        if (!editor) {
            return ctx;
        }

        QTextDocument *doc = editor->document();
        ctx.startPos = range.begin.toPositionInDocument(doc);
        ctx.endPos = range.end.toPositionInDocument(doc);
        ctx.isInsertion = (ctx.startPos == ctx.endPos);

        if (!ctx.isInsertion) {
            QTextCursor cursor(doc);
            cursor.setPosition(ctx.startPos);
            cursor.setPosition(ctx.endPos, QTextCursor::KeepAnchor);
            ctx.originalText = cursor.selectedText();
            ctx.originalText.replace(QChar(0x2029), "\n");
        } else {
            QTextCursor cursor(doc);
            cursor.setPosition(ctx.startPos);
            
            int posInBlock = cursor.positionInBlock();
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, posInBlock);
            ctx.textBeforeCursor = cursor.selectedText();
            
            cursor.setPosition(ctx.startPos);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            ctx.textAfterCursor = cursor.selectedText();
        }

        ctx.contextBefore = extractContextLines(doc, ctx.startPos, contextLinesBefore, true);
        ctx.contextAfter = extractContextLines(doc, ctx.endPos, contextLinesAfter, false);

        return ctx;
    }

private:
    static QString extractContextLines(QTextDocument *doc, int position, int lineCount, bool before)
    {
        QTextCursor cursor(doc);
        cursor.setPosition(position);
        QTextBlock currentBlock = cursor.block();

        QStringList lines;
        
        if (before) {
            QTextBlock block = currentBlock.previous();
            for (int i = 0; i < lineCount && block.isValid(); ++i) {
                lines.prepend(block.text());
                block = block.previous();
            }
        } else {
            QTextBlock block = currentBlock.next();
            for (int i = 0; i < lineCount && block.isValid(); ++i) {
                lines.append(block.text());
                block = block.next();
            }
        }

        return lines.join('\n');
    }
};

} // namespace QodeAssist

