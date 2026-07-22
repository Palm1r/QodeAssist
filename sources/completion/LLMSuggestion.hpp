// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <texteditor/texteditor.h>
#include <texteditor/textsuggestion.h>

namespace QodeAssist {

class LLMSuggestion : public TextEditor::CyclicSuggestion
{
public:
    enum Part { Word, Line };

    LLMSuggestion(
        const QList<Data> &suggestions, QTextDocument *sourceDocument, int currentCompletion = 0);

    bool applyWord(TextEditor::TextEditorWidget *widget) override;
    bool applyLine(TextEditor::TextEditorWidget *widget) override;
    bool applyPart(Part part, TextEditor::TextEditorWidget *widget);
    bool apply() override;

    static int calculateReplaceLength(const QString &suggestion, const QString &rightText);
};
} // namespace QodeAssist
