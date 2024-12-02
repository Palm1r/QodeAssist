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

#include <QObject>
#include "LSPCompletion.hpp"
#include <texteditor/textdocumentlayout.h>

#include "utils/CounterTooltip.hpp"

namespace QodeAssist {

class LLMSuggestion final : public QObject, public TextEditor::TextSuggestion
{
    Q_OBJECT
public:
    LLMSuggestion(const TextEditor::TextSuggestion::Data &data, QTextDocument *origin);

    bool apply() override;
    bool applyWord(TextEditor::TextEditorWidget *widget) override;
    bool applyLine(TextEditor::TextEditorWidget *widget) override;
    void reset();

    void showTooltip(TextEditor::TextEditorWidget *widget, int count);
    void onCounterFinished(int count);

private:
    Completion m_completion;
    QTextCursor m_start;
    int m_linesCount;

    CounterTooltip *m_counterTooltip = nullptr;
    int m_startPosition;
    TextEditor::TextSuggestion::Data m_suggestion;
};

} // namespace QodeAssist
