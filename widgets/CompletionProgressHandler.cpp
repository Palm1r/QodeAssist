// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
    if (m_progressWidget) {
        m_progressWidget->deleteLater();
        m_progressWidget = nullptr;
    }
    Utils::ToolTip::hideImmediately();
}

void CompletionProgressHandler::setCancelCallback(std::function<void()> callback)
{
    m_cancelCallback = callback;
}

void CompletionProgressHandler::identifyMatch(
    TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report)
{
    if (!editorWidget) {
        report(Priority_None);
        return;
    }

    report(Priority_Tooltip);
}

void CompletionProgressHandler::operateTooltip(
    TextEditor::TextEditorWidget *editorWidget, const QPoint &point)
{
    if (!editorWidget)
        return;

    if (m_progressWidget) {
        delete m_progressWidget;
    }

    m_progressWidget = new ProgressWidget(editorWidget);
    
    if (m_cancelCallback) {
        m_progressWidget->setCancelCallback(m_cancelCallback);
    }
    
    const QRect cursorRect = editorWidget->cursorRect(editorWidget->textCursor());
    QPoint globalPos = editorWidget->viewport()->mapToGlobal(cursorRect.topLeft());
    QPoint localPos = editorWidget->mapFromGlobal(globalPos);
    
    localPos.rx() += 5;
    localPos.ry() -= m_progressWidget->height() + 5;
    
    if (localPos.y() < 0) {
        localPos.ry() = cursorRect.bottom() + 5;
    }
    
    m_progressWidget->move(localPos);
    m_progressWidget->show();
    m_progressWidget->raise();
}

} // namespace QodeAssist
