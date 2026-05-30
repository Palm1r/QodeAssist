// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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
};

} // namespace QodeAssist

