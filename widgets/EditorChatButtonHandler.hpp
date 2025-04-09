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

#pragma once

#include "widgets/EditorChatButton.hpp"
#include <texteditor/basehoverhandler.h>
#include <QPointer>

namespace QodeAssist {

class EditorChatButtonHandler : public TextEditor::BaseHoverHandler
{
public:
    explicit EditorChatButtonHandler() = default;
    ~EditorChatButtonHandler() override;

    void showButton(TextEditor::TextEditorWidget *widget);
    void hideButton();

signals:
    void chatButtonClicked(TextEditor::TextEditorWidget *widget);

protected:
    void identifyMatch(
        TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report) override;
    void operateTooltip(TextEditor::TextEditorWidget *editorWidget, const QPoint &point) override;

private:
    QPointer<TextEditor::TextEditorWidget> m_widget;
    QPoint m_cursorPosition;
    EditorChatButton *m_chatButton = nullptr;
    int m_buttonHeight = 0;
};

} // namespace QodeAssist
