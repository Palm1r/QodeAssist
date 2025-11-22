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
    
    void hideRefactorWidget();
    
    bool isWidgetVisible() const { return !m_refactorWidget.isNull(); }

    void setApplyCallback(std::function<void(const QString &)> callback);
    void setDeclineCallback(std::function<void()> callback);

private:
    QPointer<TextEditor::TextEditorWidget> m_editor;
    QPointer<RefactorWidget> m_refactorWidget;
    
    std::function<void(const QString &)> m_applyCallback;
    std::function<void()> m_declineCallback;

    void updateWidgetPosition();
    QPoint calculateWidgetPosition();
    int getEditorWidth() const;
    QString extractIndentation(TextEditor::TextEditorWidget *editor,
                                const Utils::Text::Range &range) const;
    QString applyIndentationToFirstLine(const QString &text, const QString &indentation) const;
    QString extractContextBefore(TextEditor::TextEditorWidget *editor,
                                  const Utils::Text::Range &range,
                                  int lineCount) const;
    QString extractContextAfter(TextEditor::TextEditorWidget *editor,
                                 const Utils::Text::Range &range,
                                 int lineCount) const;
};

} // namespace QodeAssist

