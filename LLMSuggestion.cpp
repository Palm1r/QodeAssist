/*
 * Copyright (C) 2023 The Qt Company Ltd.
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * The Qt Company portions:
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
 *
 * Petr Mironychev portions:
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

#include "LLMSuggestion.hpp"
#include <texteditor/texteditor.h>
#include <utils/stringutils.h>
#include <utils/tooltip/tooltip.h>

namespace QodeAssist {

LLMSuggestion::LLMSuggestion(
    const QList<Data> &suggestions, QTextDocument *sourceDocument, int currentCompletion)
    : TextEditor::CyclicSuggestion(suggestions, sourceDocument, currentCompletion)
{
    const auto &data = suggestions[currentCompletion];

    int startPos = data.range.begin.toPositionInDocument(sourceDocument);
    int endPos = data.range.end.toPositionInDocument(sourceDocument);

    startPos = qBound(0, startPos, sourceDocument->characterCount());
    endPos = qBound(startPos, endPos, sourceDocument->characterCount());

    QTextCursor cursor(sourceDocument);
    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);

    QTextBlock block = cursor.block();
    QString blockText = block.text();

    int startPosInBlock = startPos - block.position();
    int endPosInBlock = endPos - block.position();

    blockText.replace(startPosInBlock, endPosInBlock - startPosInBlock, data.text);
    replacementDocument()->setPlainText(blockText);
}

bool LLMSuggestion::applyWord(TextEditor::TextEditorWidget *widget)
{
    return applyPart(Word, widget);
}

bool LLMSuggestion::applyLine(TextEditor::TextEditorWidget *widget)
{
    return applyPart(Line, widget);
}

bool LLMSuggestion::applyPart(Part part, TextEditor::TextEditorWidget *widget)
{
    const Utils::Text::Range range = suggestions()[currentSuggestion()].range;
    const QTextCursor cursor = range.begin.toTextCursor(sourceDocument());
    QTextCursor currentCursor = widget->textCursor();
    const QString text = suggestions()[currentSuggestion()].text;

    const int startPos = currentCursor.positionInBlock() - cursor.positionInBlock()
                         + (cursor.selectionEnd() - cursor.selectionStart());

    int next = part == Word ? Utils::endOfNextWord(text, startPos) : text.indexOf('\n', startPos);

    if (next == -1)
        return apply();

    if (part == Line)
        ++next;

    QString subText = text.mid(startPos, next - startPos);
    if (subText.isEmpty())
        return false;

    currentCursor.insertText(subText);

    if (const int seperatorPos = subText.lastIndexOf('\n'); seperatorPos >= 0) {
        const QString newCompletionText = text.mid(startPos + seperatorPos + 1);
        if (!newCompletionText.isEmpty()) {
            const Utils::Text::Position newStart{int(range.begin.line + subText.count('\n')), 0};
            const Utils::Text::Position
                newEnd{newStart.line, int(subText.length() - seperatorPos - 1)};
            const Utils::Text::Range newRange{newStart, newEnd};
            const QList<Data> newSuggestion{{newRange, newEnd, newCompletionText}};
            widget->insertSuggestion(
                std::make_unique<LLMSuggestion>(newSuggestion, widget->document(), 0));
        }
    }
    return false;
}

} // namespace QodeAssist
