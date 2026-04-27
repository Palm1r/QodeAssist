// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPointer>
#include <functional>

namespace TextEditor {
class TextEditorWidget;
}

namespace QodeAssist {

class CompletionHintWidget;

class CompletionHintHandler
{
public:
    CompletionHintHandler();
    ~CompletionHintHandler();

    void showHint(TextEditor::TextEditorWidget *widget, QPoint position, int fontSize);
    void hideHint();
    bool isHintVisible() const;
    void updateHintPosition(TextEditor::TextEditorWidget *widget, QPoint position);

private:
    QPointer<CompletionHintWidget> m_hintWidget;
};

} // namespace QodeAssist

