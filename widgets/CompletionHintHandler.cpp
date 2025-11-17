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

