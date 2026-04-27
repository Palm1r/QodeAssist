// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CompletionHintHandler.hpp"
#include "CompletionHintWidget.hpp"

#include <texteditor/texteditor.h>

namespace QodeAssist {

CompletionHintHandler::CompletionHintHandler() = default;

CompletionHintHandler::~CompletionHintHandler()
{
    hideHint();
}

void CompletionHintHandler::showHint(TextEditor::TextEditorWidget *widget, QPoint position, int fontSize)
{
    if (!widget) {
        return;
    }

    if (!m_hintWidget) {
        m_hintWidget = new CompletionHintWidget(widget, fontSize);
    }

    m_hintWidget->move(position);
    m_hintWidget->show();
    m_hintWidget->raise();
}

void CompletionHintHandler::hideHint()
{
    if (m_hintWidget) {
        m_hintWidget->deleteLater();
        m_hintWidget = nullptr;
    }
}

bool CompletionHintHandler::isHintVisible() const
{
    return !m_hintWidget.isNull() && m_hintWidget->isVisible();
}

void CompletionHintHandler::updateHintPosition(TextEditor::TextEditorWidget *widget, QPoint position)
{
    if (!widget || !m_hintWidget) {
        return;
    }

    m_hintWidget->move(position);
}

} // namespace QodeAssist

