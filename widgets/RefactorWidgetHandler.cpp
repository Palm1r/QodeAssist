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

#include <QScrollBar>
#include <QTextBlock>

#include <logger/Logger.hpp>

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
    if (!editor) {
        LOG_MESSAGE("RefactorWidgetHandler: No editor widget");
        return;
    }

    hideRefactorWidget();

    m_editor = editor;
    m_refactorWidget = new RefactorWidget(editor);
    
    QString contextBefore = extractContextBefore(editor, range, 3);
    QString contextAfter = extractContextAfter(editor, range, 3);
    
    QString baseIndentation = extractIndentation(editor, range);
    
    QString displayOriginalText = originalText;
    QString displayRefactoredText = refactoredText;
    
    if (!baseIndentation.isEmpty()) {
        displayOriginalText = applyIndentationToFirstLine(originalText, baseIndentation);
        displayRefactoredText = applyIndentationToFirstLine(refactoredText, baseIndentation);
    }
    
    m_refactorWidget->setDiffContent(displayOriginalText, displayRefactoredText, 
                                     contextBefore, contextAfter);
    
    m_refactorWidget->setApplyText(refactoredText);
    m_refactorWidget->setDisplayIndent(baseIndentation);
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
    
    LOG_MESSAGE("RefactorWidgetHandler: Showing unified diff widget (read-only)");
}

void RefactorWidgetHandler::hideRefactorWidget()
{
    if (!m_refactorWidget.isNull()) {
        if (m_editor) {
            m_editor->removeEventFilter(m_refactorWidget);
        }
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

    // Get cursor position
    QTextCursor cursor = m_editor->textCursor();
    QRect cursorRect = m_editor->cursorRect(cursor);
    
    // Convert to global coordinates
    QPoint globalPos = m_editor->mapToGlobal(cursorRect.bottomLeft());
    
    // Adjust position to avoid overlapping with cursor
    globalPos.setY(globalPos.y() + 10);
    
    // Ensure widget stays within screen bounds
    if (m_refactorWidget) {
        QRect widgetRect(globalPos, m_refactorWidget->size());
        QRect screenRect = m_editor->screen()->availableGeometry();
        
        // Move left if widget goes off right edge
        if (widgetRect.right() > screenRect.right()) {
            globalPos.setX(screenRect.right() - m_refactorWidget->width() - 10);
        }
        
        // Move up if widget goes off bottom edge
        if (widgetRect.bottom() > screenRect.bottom()) {
            globalPos.setY(m_editor->mapToGlobal(cursorRect.topLeft()).y() 
                          - m_refactorWidget->height() - 10);
        }
        
        // Ensure not too far left
        if (globalPos.x() < screenRect.left()) {
            globalPos.setX(screenRect.left() + 10);
        }
        
        // Ensure not too far up
        if (globalPos.y() < screenRect.top()) {
            globalPos.setY(screenRect.top() + 10);
        }
    }
    
    return globalPos;
}

int RefactorWidgetHandler::getEditorWidth() const
{
    if (m_editor.isNull()) {
        return 800;
    }
    
    return m_editor->viewport()->width();
}

QString RefactorWidgetHandler::extractIndentation(TextEditor::TextEditorWidget *editor,
                                                    const Utils::Text::Range &range) const
{
    if (!editor) {
        return QString();
    }
    
    QTextDocument *doc = editor->document();
    QTextBlock block = doc->findBlockByNumber(range.begin.line - 1);
    
    if (!block.isValid()) {
        return QString();
    }
    
    QString lineText = block.text();
    QString indentation;
    
    for (const QChar &ch : lineText) {
        if (ch.isSpace()) {
            indentation += ch;
        } else {
            break;
        }
    }
    
    return indentation;
}

QString RefactorWidgetHandler::applyIndentationToFirstLine(const QString &text, const QString &indentation) const
{
    if (indentation.isEmpty() || text.isEmpty()) {
        return text;
    }
    
    QStringList lines = text.split('\n');
    if (lines.isEmpty()) {
        return text;
    }
    
    lines[0] = indentation + lines[0];
    
    return lines.join('\n');
}

QString RefactorWidgetHandler::extractContextBefore(TextEditor::TextEditorWidget *editor,
                                                      const Utils::Text::Range &range,
                                                      int lineCount) const
{
    if (!editor || lineCount <= 0) {
        return QString();
    }
    
    QTextDocument *doc = editor->document();
    int startLine = range.begin.line;
    int contextStartLine = qMax(1, startLine - lineCount);
    
    QStringList contextLines;
    for (int line = contextStartLine; line < startLine; ++line) {
        QTextBlock block = doc->findBlockByNumber(line - 1);
        if (block.isValid()) {
            contextLines.append(block.text());
        }
    }
    
    return contextLines.join('\n');
}

QString RefactorWidgetHandler::extractContextAfter(TextEditor::TextEditorWidget *editor,
                                                     const Utils::Text::Range &range,
                                                     int lineCount) const
{
    if (!editor || lineCount <= 0) {
        return QString();
    }
    
    QTextDocument *doc = editor->document();
    int endLine = range.end.line;
    int totalLines = doc->blockCount();
    int contextEndLine = qMin(totalLines, endLine + lineCount);
    
    QStringList contextLines;
    for (int line = endLine + 1; line <= contextEndLine; ++line) {
        QTextBlock block = doc->findBlockByNumber(line - 1);
        if (block.isValid()) {
            contextLines.append(block.text());
        }
    }
    
    return contextLines.join('\n');
}

} // namespace QodeAssist

