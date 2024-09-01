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

#include "LLMSuggestion.hpp"

#include <texteditor/texteditor.h>
#include <utils/stringutils.h>

namespace QodeAssist {

LLMSuggestion::LLMSuggestion(const Completion &completion, QTextDocument *origin)
    : m_completion(completion)
{
    int startPos = completion.range().start().toPositionInDocument(origin);
    int endPos = completion.range().end().toPositionInDocument(origin);

    startPos = qBound(0, startPos, origin->characterCount() - 1);
    endPos = qBound(startPos, endPos, origin->characterCount() - 1);

    m_start = QTextCursor(origin);
    m_start.setPosition(startPos);
    m_start.setKeepPositionOnInsert(true);

    QTextCursor cursor(origin);
    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);

    QTextBlock block = cursor.block();
    QString blockText = block.text();

    int startPosInBlock = startPos - block.position();
    int endPosInBlock = endPos - block.position();

    blockText.replace(startPosInBlock, endPosInBlock - startPosInBlock, completion.text());

    document()->setPlainText(blockText);

    setCurrentPosition(m_start.position());
}

bool LLMSuggestion::apply()
{
    QTextCursor cursor = m_completion.range().toSelection(m_start.document());
    cursor.beginEditBlock();
    cursor.removeSelectedText();
    cursor.insertText(m_completion.text());
    cursor.endEditBlock();
    return true;
}

bool LLMSuggestion::applyWord(TextEditor::TextEditorWidget *widget)
{
    return applyNextLine(widget);
}

bool LLMSuggestion::applyNextLine(TextEditor::TextEditorWidget *widget)
{
    QTextCursor currentCursor = widget->textCursor();
    const QString text = m_completion.text();

    int endPos = currentCursor.position();
    while (endPos < text.length() && !text[endPos].isSpace()) {
        ++endPos;
    }

    QString wordToInsert = text.left(endPos);

    wordToInsert = wordToInsert.split('\n').first();

    if (!wordToInsert.isEmpty()) {
        currentCursor.insertText(wordToInsert);
        widget->setTextCursor(currentCursor);
    }

    return false;
}

void LLMSuggestion::reset()
{
    m_start.removeSelectedText();
}

int LLMSuggestion::position()
{
    return m_start.position();
}

} // namespace QodeAssist
