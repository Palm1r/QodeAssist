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

