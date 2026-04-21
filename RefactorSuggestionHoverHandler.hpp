// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

