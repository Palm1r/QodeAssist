// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <texteditor/basehoverhandler.h>
#include <QPointer>
#include <functional>

namespace QodeAssist {

class ProgressWidget;

class CompletionProgressHandler : public TextEditor::BaseHoverHandler
{
public:
    void showProgress(TextEditor::TextEditorWidget *widget);
    void hideProgress();
    void setCancelCallback(std::function<void()> callback);
    bool isProgressVisible() const { return !m_progressWidget.isNull(); }

protected:
    void identifyMatch(
        TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report) override;
    void operateTooltip(TextEditor::TextEditorWidget *editorWidget, const QPoint &point) override;

private:
    QPointer<TextEditor::TextEditorWidget> m_widget;
    QPointer<ProgressWidget> m_progressWidget;
    QPoint m_iconPosition;
    std::function<void()> m_cancelCallback;
};

} // namespace QodeAssist
