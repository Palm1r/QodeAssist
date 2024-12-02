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

#include <QTextCursor>
#include <QtWidgets/qtoolbar.h>
#include <texteditor/texteditor.h>
#include <utils/stringutils.h>
#include <utils/tooltip/tooltip.h>

namespace QodeAssist {

LLMSuggestion::LLMSuggestion(const TextEditor::TextSuggestion::Data &data, QTextDocument *origin)
    : TextEditor::TextSuggestion(data, origin)
    , m_linesCount(0)
    , m_suggestion(data)
{
    int startPos = data.range.begin.toPositionInDocument(origin);
    int endPos = data.range.end.toPositionInDocument(origin);

    startPos = qBound(0, startPos, origin->characterCount() - 1);
    endPos = qBound(startPos, endPos, origin->characterCount() - 1);

    QTextCursor cursor(origin);
    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);

    QTextBlock block = cursor.block();
    QString blockText = block.text();

    int startPosInBlock = startPos - block.position();
    int endPosInBlock = endPos - block.position();

    blockText.replace(startPosInBlock, endPosInBlock - startPosInBlock, data.text);

    replacementDocument()->setPlainText(blockText);
}

bool LLMSuggestion::apply()
{
    return TextEditor::TextSuggestion::apply();
}

bool LLMSuggestion::applyWord(TextEditor::TextEditorWidget *widget)
{
    return TextEditor::TextSuggestion::applyWord(widget);
}

bool LLMSuggestion::applyLine(TextEditor::TextEditorWidget *widget)
{
    return TextEditor::TextSuggestion::applyLine(widget);
}

void LLMSuggestion::reset()
{
    reset();
    m_linesCount = 0;
}

void LLMSuggestion::onCounterFinished(int count)
{
    Utils::ToolTip::hide();
    m_linesCount = 0;
    QTextCursor cursor = m_completion.range().toSelection(m_start.document());
    cursor.beginEditBlock();
    cursor.removeSelectedText();

    QStringList lines = m_completion.text().split('\n');
    QString textToInsert = lines.mid(0, count).join('\n');

    cursor.insertText(textToInsert);
    cursor.endEditBlock();
}

// void LLMSuggestion::reset()
// {
//     m_start.removeSelectedText();
// }

// int LLMSuggestion::position()
// {
//     return m_start.position();
// }

void LLMSuggestion::showTooltip(TextEditor::TextEditorWidget *widget, int count)
{
    // Utils::ToolTip::hide();
    // QPoint pos = widget->mapToGlobal(widget->cursorRect().topRight());
    // pos += QPoint(-10, -50);
    // m_counterTooltip = new CounterTooltip(count);
    // Utils::ToolTip::show(pos, m_counterTooltip, widget);
    // connect(m_counterTooltip, &CounterTooltip::finished, this, &LLMSuggestion::onCounterFinished);
}

} // namespace QodeAssist
