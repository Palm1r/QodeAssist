// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <texteditor/texteditor.h>
#include <texteditor/textsuggestion.h>

namespace QodeAssist {

class RefactorSuggestion : public TextEditor::TextSuggestion
{
public:
    RefactorSuggestion(const Data &suggestion, QTextDocument *sourceDocument);

    bool apply() override;

    bool applyWord(TextEditor::TextEditorWidget *widget) override;

    bool applyLine(TextEditor::TextEditorWidget *widget) override;

private:
    Data m_suggestionData;
};

} // namespace QodeAssist

