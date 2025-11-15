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

#include "CompletionErrorHandler.hpp"

#include <texteditor/texteditor.h>
#include <utils/tooltip/tooltip.h>

#include "ErrorWidget.hpp"

namespace QodeAssist {

void CompletionErrorHandler::showError(
    TextEditor::TextEditorWidget *widget, 
    const QString &errorMessage, 
    int autoHideMs)
{
    m_widget = widget;
    m_errorMessage = errorMessage;

    if (m_widget) {
        const QRect cursorRect = m_widget->cursorRect(m_widget->textCursor());
        m_errorPosition = m_widget->viewport()->mapToGlobal(cursorRect.topLeft())
                          - Utils::ToolTip::offsetFromPosition();

        identifyMatch(m_widget, m_widget->textCursor().position(), [this, autoHideMs](auto priority) {
            if (priority != Priority_None) {
                if (m_errorWidget) {
                    m_errorWidget->deleteLater();
                }

                m_errorWidget = new ErrorWidget(m_errorMessage, m_widget);
                
                const QRect cursorRect = m_widget->cursorRect(m_widget->textCursor());
                QPoint globalPos = m_widget->viewport()->mapToGlobal(cursorRect.topLeft());
                QPoint localPos = m_widget->mapFromGlobal(globalPos);
                localPos.ry() -= m_errorWidget->height() + 5;
                
                if (localPos.y() < 0) {
                    localPos.ry() = cursorRect.bottom() + 5;
                }
                
                m_errorWidget->move(localPos);
                m_errorWidget->show();
                m_errorWidget->raise();

                QObject::connect(m_errorWidget, &ErrorWidget::dismissed, m_errorWidget, [this]() {
                    hideError();
                });
            }
        });
    }
}

void CompletionErrorHandler::hideError()
{
    if (m_errorWidget) {
        m_errorWidget->deleteLater();
        m_errorWidget = nullptr;
    }
    Utils::ToolTip::hideImmediately();
    m_errorMessage.clear();
}

void CompletionErrorHandler::identifyMatch(
    TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report)
{
    if (!editorWidget || m_errorMessage.isEmpty()) {
        report(Priority_None);
        return;
    }

    report(Priority_Tooltip);
}

void CompletionErrorHandler::operateTooltip(
    TextEditor::TextEditorWidget *editorWidget, const QPoint &point)
{
    Q_UNUSED(editorWidget)
    Q_UNUSED(point)
}

} // namespace QodeAssist

