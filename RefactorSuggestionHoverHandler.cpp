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

#include "RefactorSuggestionHoverHandler.hpp"
#include "RefactorSuggestion.hpp"

#include <QColor>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScopeGuard>
#include <QTextBlock>
#include <QTextCursor>
#include <QWidget>

#include <texteditor/textdocumentlayout.h>
#include <texteditor/texteditor.h>
#include <utils/theme/theme.h>
#include <utils/tooltip/tooltip.h>

#include <logger/Logger.hpp>

namespace QodeAssist {

RefactorSuggestionHoverHandler::RefactorSuggestionHoverHandler()
{
    setPriority(Priority_Suggestion);
}

void RefactorSuggestionHoverHandler::setSuggestionRange(const Utils::Text::Range &range)
{
    m_suggestionRange = range;
    m_hasSuggestion = true;
}

void RefactorSuggestionHoverHandler::clearSuggestionRange()
{
    m_hasSuggestion = false;
}

void RefactorSuggestionHoverHandler::identifyMatch(
    TextEditor::TextEditorWidget *editorWidget,
    int pos,
    ReportPriority report)
{
    
    QScopeGuard cleanup([&] { report(Priority_None); });
    
    if (!editorWidget->suggestionVisible()) {
        return;
    }

    QTextCursor cursor(editorWidget->document());
    cursor.setPosition(pos);
    m_block = cursor.block();
    
#if QODEASSIST_QT_CREATOR_VERSION_MAJOR >= 17
    auto *suggestion = dynamic_cast<RefactorSuggestion *>(
        TextEditor::TextBlockUserData::suggestion(m_block));
#else
    auto *userData = TextEditor::TextDocumentLayout::textUserData(m_block);
    if (!userData) {
        LOG_MESSAGE("RefactorSuggestionHoverHandler: No user data in block");
        return;
    }
    
    auto *suggestion = dynamic_cast<RefactorSuggestion *>(userData->suggestion());
#endif
    
    if (!suggestion) {
        return;
    }

    cleanup.dismiss();
    report(Priority_Suggestion);
}

void RefactorSuggestionHoverHandler::operateTooltip(
    TextEditor::TextEditorWidget *editorWidget,
    const QPoint &point)
{
    Q_UNUSED(point)
    
#if QODEASSIST_QT_CREATOR_VERSION_MAJOR >= 17
    auto *suggestion = dynamic_cast<RefactorSuggestion *>(
        TextEditor::TextBlockUserData::suggestion(m_block));
#else
    auto *userData = TextEditor::TextDocumentLayout::textUserData(m_block);
    if (!userData) {
        LOG_MESSAGE("RefactorSuggestionHoverHandler::operateTooltip: No user data in block");
        return;
    }
    
    auto *suggestion = dynamic_cast<RefactorSuggestion *>(userData->suggestion());
#endif

    if (!suggestion) {
        return;
    }

    auto *widget = new QWidget();
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(4, 3, 4, 3);
    layout->setSpacing(6);

    const QColor normalBg = Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
    const QColor hoverBg = Utils::creatorColor(Utils::Theme::BackgroundColorHover);
    const QColor selectedBg = Utils::creatorColor(Utils::Theme::BackgroundColorSelected);
    const QColor textColor = Utils::creatorColor(Utils::Theme::TextColorNormal);
    const QColor borderColor = Utils::creatorColor(Utils::Theme::SplitterColor);
    const QColor successColor = Utils::creatorColor(Utils::Theme::TextColorNormal);
    const QColor errorColor = Utils::creatorColor(Utils::Theme::TextColorError);

    auto *applyButton = new QPushButton("✓ Apply", widget);
    applyButton->setFocusPolicy(Qt::NoFocus);
    applyButton->setToolTip("Apply refactoring (Tab)");
    applyButton->setCursor(Qt::PointingHandCursor);
    applyButton->setStyleSheet(QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 3px;"
        "  padding: 4px 12px;"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "  min-width: 60px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %4;"
        "  border-color: %2;"
        "}"
        "QPushButton:pressed {"
        "  background-color: %5;"
        "}")
        .arg(selectedBg.name())
        .arg(successColor.name())
        .arg(borderColor.name())
        .arg(selectedBg.lighter(110).name())
        .arg(selectedBg.darker(110).name()));
    QObject::connect(applyButton, &QPushButton::clicked, widget, [this]() {
        Utils::ToolTip::hide();
        if (m_applyCallback) {
            m_applyCallback();
        }
    });

    auto *dismissButton = new QPushButton("✕ Dismiss", widget);
    dismissButton->setFocusPolicy(Qt::NoFocus);
    dismissButton->setToolTip("Dismiss refactoring (Esc)");
    dismissButton->setCursor(Qt::PointingHandCursor);
    dismissButton->setStyleSheet(QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 3px;"
        "  padding: 4px 12px;"
        "  font-size: 11px;"
        "  min-width: 60px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %4;"
        "  color: %5;"
        "  border-color: %5;"
        "}"
        "QPushButton:pressed {"
        "  background-color: %6;"
        "}")
        .arg(normalBg.name())
        .arg(textColor.name())
        .arg(borderColor.name())
        .arg(hoverBg.name())
        .arg(errorColor.name())
        .arg(hoverBg.darker(110).name()));
    QObject::connect(dismissButton, &QPushButton::clicked, widget, [this]() {
        Utils::ToolTip::hide();
        if (m_dismissCallback) {
            m_dismissCallback();
        }
    });

    layout->addWidget(applyButton);
    layout->addWidget(dismissButton);

    const QRect cursorRect = editorWidget->cursorRect(editorWidget->textCursor());
    QPoint pos = editorWidget->viewport()->mapToGlobal(cursorRect.topLeft())
                 - Utils::ToolTip::offsetFromPosition();
    pos.ry() -= widget->sizeHint().height();
    
    Utils::ToolTip::show(pos, widget, editorWidget);
}

} // namespace QodeAssist

