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

#include <functional>
#include <QTextBlock>

#include <texteditor/basehoverhandler.h>
#include <utils/textutils.h>

namespace TextEditor {
class TextEditorWidget;
}

namespace QodeAssist {

/**
 * @brief Hover handler for refactoring suggestions
 * 
 * Shows interactive tooltip with Apply/Dismiss buttons when hovering over
 * a refactoring suggestion in the editor.
 */
class RefactorSuggestionHoverHandler : public TextEditor::BaseHoverHandler
{
public:
    using ApplyCallback = std::function<void()>;
    using DismissCallback = std::function<void()>;

    RefactorSuggestionHoverHandler();

    void setSuggestionRange(const Utils::Text::Range &range);
    void clearSuggestionRange();
    bool hasSuggestion() const { return m_hasSuggestion; }

    void setApplyCallback(ApplyCallback callback) { m_applyCallback = std::move(callback); }
    void setDismissCallback(DismissCallback callback) { m_dismissCallback = std::move(callback); }

protected:
    void identifyMatch(TextEditor::TextEditorWidget *editorWidget,
                       int pos,
                       ReportPriority report) override;
    
    void operateTooltip(TextEditor::TextEditorWidget *editorWidget, 
                       const QPoint &point) override;

private:
    Utils::Text::Range m_suggestionRange;
    bool m_hasSuggestion = false;
    ApplyCallback m_applyCallback;
    DismissCallback m_dismissCallback;
    QTextBlock m_block;
};

} // namespace QodeAssist

