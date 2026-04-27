// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
