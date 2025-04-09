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

#include "EditorChatButtonHandler.hpp"
#include "EditorChatButton.hpp"

#include <texteditor/texteditor.h>
#include <utils/tooltip/tooltip.h>
#include <QPoint>

namespace QodeAssist {

EditorChatButtonHandler::~EditorChatButtonHandler()
{
    delete m_chatButton;
}

void EditorChatButtonHandler::showButton(TextEditor::TextEditorWidget *widget)
{
    if (!widget)
        return;

    m_widget = widget;

    identifyMatch(widget, widget->textCursor().position(), [this](auto priority) {
        if (priority != Priority_None && m_widget) {
            const QTextCursor cursor = m_widget->textCursor();
            const QRect selectionRect = m_widget->cursorRect(cursor);
            m_cursorPosition = m_widget->viewport()->mapToGlobal(selectionRect.topLeft())
                               - Utils::ToolTip::offsetFromPosition();
            operateTooltip(m_widget, m_cursorPosition);
        }
    });
}

void EditorChatButtonHandler::hideButton()
{
    Utils::ToolTip::hide();
}

void EditorChatButtonHandler::identifyMatch(
    TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report)
{
    if (!editorWidget) {
        report(Priority_None);
        return;
    }

    report(Priority_Tooltip);
}

void EditorChatButtonHandler::operateTooltip(
    TextEditor::TextEditorWidget *editorWidget, const QPoint &point)
{
    if (!editorWidget)
        return;

    if (!Utils::ToolTip::isVisible()) {
        m_chatButton = new EditorChatButton(editorWidget);
        m_buttonHeight = m_chatButton->height();

        QPoint showPoint = point;
        showPoint.ry() -= m_buttonHeight;

        Utils::ToolTip::show(showPoint, m_chatButton, editorWidget);
    } else {
        QPoint showPoint = point;
        showPoint.ry() -= m_buttonHeight;
        Utils::ToolTip::move(showPoint);
    }
}

} // namespace QodeAssist
