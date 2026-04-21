// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPointer>
#include <functional>

#include <texteditor/texteditor.h>
#include <utils/textutils.h>

namespace QodeAssist {

class RefactorWidget;

class RefactorWidgetHandler : public QObject
{
    Q_OBJECT

public:
    explicit RefactorWidgetHandler(QObject *parent = nullptr);
    ~RefactorWidgetHandler() override;

    void showRefactorWidget(
        TextEditor::TextEditorWidget *editor,
        const QString &originalText,
        const QString &refactoredText,
        const Utils::Text::Range &range);
    
    void showRefactorWidget(
        TextEditor::TextEditorWidget *editor,
        const QString &originalText,
        const QString &refactoredText,
        const Utils::Text::Range &range,
        const QString &contextBefore,
        const QString &contextAfter);
    
    void hideRefactorWidget();
    
    bool isWidgetVisible() const { return !m_refactorWidget.isNull(); }

    void setApplyCallback(std::function<void(const QString &)> callback);
    void setDeclineCallback(std::function<void()> callback);
    void setTextToApply(const QString &text);

private:
    QPointer<TextEditor::TextEditorWidget> m_editor;
    QPointer<RefactorWidget> m_refactorWidget;
    
    std::function<void(const QString &)> m_applyCallback;
    std::function<void()> m_declineCallback;

    void updateWidgetPosition();
    QPoint calculateWidgetPosition();
    int getEditorWidth() const;
};

} // namespace QodeAssist

