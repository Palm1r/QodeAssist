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

#include "RefactorWidgetHandler.hpp"
#include "RefactorWidget.hpp"
#include "ContextExtractor.hpp"

#include <QScrollBar>
#include <QTextBlock>

namespace QodeAssist {

RefactorWidgetHandler::RefactorWidgetHandler(QObject *parent)
    : QObject(parent)
{
}

RefactorWidgetHandler::~RefactorWidgetHandler()
{
    hideRefactorWidget();
}

void RefactorWidgetHandler::showRefactorWidget(
    TextEditor::TextEditorWidget *editor,
    const QString &originalText,
    const QString &refactoredText,
    const Utils::Text::Range &range)
{
    QString contextBefore = ContextExtractor::extractBefore(editor, range, 3);
    QString contextAfter = ContextExtractor::extractAfter(editor, range, 3);
    showRefactorWidget(editor, originalText, refactoredText, range, contextBefore, contextAfter);
}

void RefactorWidgetHandler::showRefactorWidget(
    TextEditor::TextEditorWidget *editor,
    const QString &originalText,
    const QString &refactoredText,
    const Utils::Text::Range &range,
    const QString &contextBefore,
    const QString &contextAfter)
{
    if (!editor) {
        return;
    }

    hideRefactorWidget();

    m_editor = editor;
    m_refactorWidget = new RefactorWidget(editor);
    
    m_refactorWidget->setDiffContent(originalText, refactoredText, contextBefore, contextAfter);
    m_refactorWidget->setApplyText(refactoredText);
    m_refactorWidget->setRange(range);
    m_refactorWidget->setEditorWidth(getEditorWidth());
    
    if (m_applyCallback) {
        m_refactorWidget->setApplyCallback(m_applyCallback);
    }
    
    if (m_declineCallback) {
        m_refactorWidget->setDeclineCallback(m_declineCallback);
    }

    updateWidgetPosition();
    m_refactorWidget->show();
    m_refactorWidget->raise();
}

void RefactorWidgetHandler::hideRefactorWidget()
{
    if (!m_refactorWidget.isNull()) {
        m_refactorWidget->close();
        m_refactorWidget = nullptr;
    }
    m_editor = nullptr;
}

void RefactorWidgetHandler::setApplyCallback(std::function<void(const QString &)> callback)
{
    m_applyCallback = callback;
}

void RefactorWidgetHandler::setDeclineCallback(std::function<void()> callback)
{
    m_declineCallback = callback;
}

void RefactorWidgetHandler::setTextToApply(const QString &text)
{
    if (!m_refactorWidget.isNull()) {
        m_refactorWidget->setApplyText(text);
    }
}

void RefactorWidgetHandler::updateWidgetPosition()
{
    if (m_refactorWidget.isNull() || m_editor.isNull()) {
        return;
    }

    QPoint position = calculateWidgetPosition();
    m_refactorWidget->move(position);
}

QPoint RefactorWidgetHandler::calculateWidgetPosition()
{
    if (m_editor.isNull()) {
        return QPoint(0, 0);
    }

    QTextCursor cursor = m_editor->textCursor();
    QRect cursorRect = m_editor->cursorRect(cursor);
    QPoint globalPos = m_editor->mapToGlobal(cursorRect.bottomLeft());
    globalPos.setY(globalPos.y() + 10);
    
    if (m_refactorWidget) {
        QRect widgetRect(globalPos, m_refactorWidget->size());
        QRect screenRect = m_editor->screen()->availableGeometry();
        
        if (widgetRect.right() > screenRect.right()) {
            globalPos.setX(screenRect.right() - m_refactorWidget->width() - 10);
        }
        
        if (widgetRect.bottom() > screenRect.bottom()) {
            globalPos.setY(m_editor->mapToGlobal(cursorRect.topLeft()).y() 
                          - m_refactorWidget->height() - 10);
        }
        
        if (globalPos.x() < screenRect.left()) {
            globalPos.setX(screenRect.left() + 10);
        }
        
        if (globalPos.y() < screenRect.top()) {
            globalPos.setY(screenRect.top() + 10);
        }
    }
    
    return globalPos;
}

int RefactorWidgetHandler::getEditorWidth() const
{
    return m_editor.isNull() ? 800 : m_editor->viewport()->width();
}

} // namespace QodeAssist

