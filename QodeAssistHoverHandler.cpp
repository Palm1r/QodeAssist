/*
 * Copyright (C) 2023 The Qt Company Ltd.
 * Copyright (C) 2024 Petr Mironychev
 *
 * This file is part of Qode Assist.
 *
 * The Qt Company portions:
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
 *
 * Petr Mironychev portions:
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

#include "QodeAssistHoverHandler.hpp"

#include <QPushButton>
#include <QScopeGuard>
#include <QToolBar>
#include <QToolButton>

#include <texteditor/textdocument.h>
#include <texteditor/textdocumentlayout.h>
#include <texteditor/texteditor.h>

#include <utils/tooltip/tooltip.h>
#include <utils/utilsicons.h>

#include "LLMSuggestion.hpp"
#include "LSPCompletion.hpp"
#include "QodeAssisttr.h"

using namespace TextEditor;
using namespace LanguageServerProtocol;
using namespace Utils;

namespace QodeAssist {

class QodeAssistCompletionToolTip : public QToolBar
{
public:
    QodeAssistCompletionToolTip(TextEditorWidget *editor)
        : m_editor(editor)
    {
        auto apply = addAction(Tr::tr("Apply (%1)").arg(QKeySequence(Qt::Key_Tab).toString()));
        connect(apply, &QAction::triggered, this, &QodeAssistCompletionToolTip::apply);

        auto applyWord = addAction(Tr::tr("Apply Next Line (%1)")
                                       .arg(QKeySequence(QKeySequence::MoveToNextLine).toString()));
        connect(applyWord, &QAction::triggered, this, &QodeAssistCompletionToolTip::applyWord);
        qDebug() << "applyWord sequence" << applyWord->shortcut();
    }

private:
    void apply()
    {
        if (auto *suggestion = dynamic_cast<LLMSuggestion *>(m_editor->currentSuggestion())) {
            if (!suggestion->apply())
                return;
        }
        ToolTip::hide();
    }

    void applyWord()
    {
        if (auto *suggestion = dynamic_cast<LLMSuggestion *>(m_editor->currentSuggestion())) {
            if (!suggestion->applyWord(m_editor))
                return;
        }
        ToolTip::hide();
    }

    TextEditorWidget *m_editor;
};

void QodeAssistHoverHandler::identifyMatch(TextEditor::TextEditorWidget *editorWidget,
                                           int pos,
                                           ReportPriority report)
{
    QScopeGuard cleanup([&] { report(Priority_None); });
    if (!editorWidget->suggestionVisible())
        return;

    QTextCursor cursor(editorWidget->document());
    cursor.setPosition(pos);
    m_block = cursor.block();
    auto *suggestion = dynamic_cast<LLMSuggestion *>(TextDocumentLayout::suggestion(m_block));

    if (!suggestion)
        return;

    const Completion completion = suggestion->completion();
    if (completion.text().isEmpty())
        return;

    cleanup.dismiss();
    report(Priority_Suggestion);
}

void QodeAssistHoverHandler::operateTooltip(TextEditor::TextEditorWidget *editorWidget,
                                            const QPoint &point)
{
    Q_UNUSED(point)
    auto *suggestion = dynamic_cast<LLMSuggestion *>(TextDocumentLayout::suggestion(m_block));

    if (!suggestion)
        return;

    auto tooltipWidget = new QodeAssistCompletionToolTip(editorWidget);

    const QRect cursorRect = editorWidget->cursorRect(editorWidget->textCursor());
    QPoint pos = editorWidget->viewport()->mapToGlobal(cursorRect.topLeft())
                 - Utils::ToolTip::offsetFromPosition();
    pos.ry() -= tooltipWidget->sizeHint().height();
    ToolTip::show(pos, tooltipWidget, editorWidget);
}

} // namespace QodeAssist
