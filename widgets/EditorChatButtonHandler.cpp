// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
