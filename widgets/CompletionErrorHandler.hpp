// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <texteditor/basehoverhandler.h>
#include <QPointer>

namespace QodeAssist {

class ErrorWidget;

class CompletionErrorHandler : public TextEditor::BaseHoverHandler
{
public:
    void showError(
        TextEditor::TextEditorWidget *widget, 
        const QString &errorMessage, 
        int autoHideMs = 5000);

    void hideError();

    bool isErrorVisible() const { return !m_errorWidget.isNull(); }

protected:
    void identifyMatch(
        TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report) override;
    void operateTooltip(TextEditor::TextEditorWidget *editorWidget, const QPoint &point) override;

private:
    QPointer<TextEditor::TextEditorWidget> m_widget;
    QPointer<ErrorWidget> m_errorWidget;
    QString m_errorMessage;
    QPoint m_errorPosition;
};

} // namespace QodeAssist

