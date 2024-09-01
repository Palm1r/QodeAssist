/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include <texteditor/textdocumentlayout.h>

#include "LSPCompletion.hpp"

namespace QodeAssist {

class LLMSuggestion final : public TextEditor::TextSuggestion
{
public:
    LLMSuggestion(const Completion &completion, QTextDocument *origin);

    bool apply() final;
    bool applyWord(TextEditor::TextEditorWidget *widget) final;
    bool applyNextLine(TextEditor::TextEditorWidget *widget);
    void reset() final;
    int position() final;

    const Completion &completion() const { return m_completion; }

private:
    Completion m_completion;
    QTextCursor m_start;
};

} // namespace QodeAssist
