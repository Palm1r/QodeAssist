/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include <texteditor/texteditor.h>
#include <texteditor/textsuggestion.h>

namespace QodeAssist {

/**
 * @brief Persistent refactoring suggestion that displays code changes inline
 * 
 * Unlike LLMSuggestion which supports partial acceptance (word/line), 
 * RefactorSuggestion is designed to show complete refactoring results 
 * that must be either fully accepted or rejected by the user.
 */
class RefactorSuggestion : public TextEditor::TextSuggestion
{
public:
    /**
     * @brief Constructs a refactoring suggestion
     * @param suggestion Suggestion data (range, position, text)
     * @param sourceDocument The document where suggestion will be displayed
     */
    RefactorSuggestion(const Data &suggestion, QTextDocument *sourceDocument);

    /**
     * @brief Applies the full refactoring suggestion with smart overlapping
     * @return true if suggestion was applied successfully
     */
    bool apply() override;

    /**
     * @brief Disabled: Word-by-word acceptance not supported for refactoring
     */
    bool applyWord(TextEditor::TextEditorWidget *widget) override;

    /**
     * @brief Disabled: Line-by-line acceptance not supported for refactoring
     */
    bool applyLine(TextEditor::TextEditorWidget *widget) override;

private:
    Data m_suggestionData;
};

} // namespace QodeAssist

