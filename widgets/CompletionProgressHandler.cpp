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

#include "CompletionProgressHandler.hpp"

#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include <texteditor/texteditor.h>
#include <utils/progressindicator.h>
#include <utils/theme/theme.h>
#include <utils/tooltip/tooltip.h>

#include "ProgressWidget.hpp"

namespace QodeAssist {

void CompletionProgressHandler::showProgress(TextEditor::TextEditorWidget *widget)
{
    m_widget = widget;
    m_isActive = true;

    if (m_widget) {
        const QRect cursorRect = m_widget->cursorRect(m_widget->textCursor());
        m_iconPosition = m_widget->viewport()->mapToGlobal(cursorRect.topLeft())
                         - Utils::ToolTip::offsetFromPosition();

        identifyMatch(m_widget, m_widget->textCursor().position(), [this](auto priority) {
            if (priority != Priority_None)
                operateTooltip(m_widget, m_iconPosition);
        });
    }
}

void CompletionProgressHandler::hideProgress()
{
    m_isActive = false;
    Utils::ToolTip::hide();
}

void CompletionProgressHandler::identifyMatch(
    TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report)
{
    if (!m_isActive || !editorWidget) {
        report(Priority_None);
        return;
    }

    report(Priority_Tooltip);
}

void CompletionProgressHandler::operateTooltip(
    TextEditor::TextEditorWidget *editorWidget, const QPoint &point)
{
    if (!m_isActive || !editorWidget)
        return;

    auto progressWidget = new ProgressWidget(editorWidget);

    QPoint showPoint = point;
    showPoint.ry() -= progressWidget->height();

    Utils::ToolTip::show(showPoint, progressWidget, editorWidget);
}

} // namespace QodeAssist
