/*
 * Copyright (C) 2023 The Qt Company Ltd.
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * The Qt Company portions:
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
 *
 * Petr Mironychev portions:
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

#include "LLMSuggestion.hpp"
#include <texteditor/texteditor.h>
#include <utils/stringutils.h>
#include <utils/tooltip/tooltip.h>

namespace QodeAssist {

static QStringList extractTokens(const QString &str)
{
    QStringList tokens;
    QString currentToken;
    for (const QChar &ch : str) {
        if (ch.isLetterOrNumber() || ch == '_') {
            currentToken += ch;
        } else {
            if (!currentToken.isEmpty() && currentToken.length() > 1) {
                tokens.append(currentToken);
            }
            currentToken.clear();
        }
    }
    if (!currentToken.isEmpty() && currentToken.length() > 1) {
        tokens.append(currentToken);
    }
    return tokens;
}

int LLMSuggestion::calculateReplaceLength(const QString &suggestion, 
                                         const QString &rightText,
                                         const QString &entireLine)
{
    if (rightText.isEmpty()) {
        return 0;
    }

    QString structuralChars = "{}[]()<>;,";
    bool hasStructuralOverlap = false;
    for (const QChar &ch : structuralChars) {
        if (suggestion.contains(ch) && rightText.contains(ch)) {
            hasStructuralOverlap = true;
            break;
        }
    }
    
    if (hasStructuralOverlap) {
        return rightText.length();
    }

    const QStringList suggestionTokens = extractTokens(suggestion);
    const QStringList lineTokens = extractTokens(entireLine);
    
    for (const auto &token : suggestionTokens) {
        if (lineTokens.contains(token)) {
            return rightText.length();
        }
    }
    
    return 0;
}

LLMSuggestion::LLMSuggestion(
    const QList<Data> &suggestions, QTextDocument *sourceDocument, int currentCompletion)
    : TextEditor::CyclicSuggestion(suggestions, sourceDocument, currentCompletion)
{
    const auto &data = suggestions[currentCompletion];

    int startPos = data.range.begin.toPositionInDocument(sourceDocument);

    startPos = qBound(0, startPos, sourceDocument->characterCount());

    QTextCursor cursor(sourceDocument);
    cursor.setPosition(startPos);
    QTextBlock block = cursor.block();
    QString blockText = block.text();

    int cursorPositionInBlock = cursor.positionInBlock();
    QString leftText = blockText.left(cursorPositionInBlock);
    QString rightText = blockText.mid(cursorPositionInBlock);

    QString suggestionText = data.text;
    QString entireLine = blockText;

    if (!suggestionText.contains('\n')) {
        int replaceLength = calculateReplaceLength(suggestionText, rightText, entireLine);
        QString remainingRightText = (replaceLength > 0) ? rightText.mid(replaceLength) : rightText;
        
        QString displayText = leftText + suggestionText + remainingRightText;
        replacementDocument()->setPlainText(displayText);
    } else {
        int firstLineEnd = suggestionText.indexOf('\n');
        QString firstLine = suggestionText.left(firstLineEnd);
        QString restOfCompletion = suggestionText.mid(firstLineEnd);
        
        int replaceLength = calculateReplaceLength(firstLine, rightText, entireLine);
        QString remainingRightText = (replaceLength > 0) ? rightText.mid(replaceLength) : rightText;
        
        QString displayText = leftText + firstLine + remainingRightText + restOfCompletion;
        replacementDocument()->setPlainText(displayText);
    }
}

bool LLMSuggestion::applyWord(TextEditor::TextEditorWidget *widget)
{
    return applyPart(Word, widget);
}

bool LLMSuggestion::applyLine(TextEditor::TextEditorWidget *widget)
{
    return applyPart(Line, widget);
}

bool LLMSuggestion::applyPart(Part part, TextEditor::TextEditorWidget *widget)
{
    const auto &currentSuggestions = suggestions();
    const auto &currentData = currentSuggestions[currentSuggestion()];
    const Utils::Text::Range range = currentData.range;
    const QTextCursor cursor = range.begin.toTextCursor(sourceDocument());
    QTextCursor currentCursor = widget->textCursor();
    const QString text = currentData.text;

    const int startPos = currentCursor.positionInBlock() - cursor.positionInBlock()
                         + (cursor.selectionEnd() - cursor.selectionStart());

    int next = part == Word ? Utils::endOfNextWord(text, startPos) : text.indexOf('\n', startPos);

    if (next == -1) {
        if (part == Line) {
            next = text.length();
        } else {
            return apply();
        }
    }

    if (part == Line)
        ++next;

    QString subText = text.mid(startPos, next - startPos);

    if (subText.isEmpty()) {
        return false;
    }

    if (startPos == 0) {
        QTextBlock currentBlock = cursor.block();
        QString textAfterCursor = currentBlock.text().mid(cursor.positionInBlock());
        QString entireLine = currentBlock.text();
        
        int replaceLength = calculateReplaceLength(text, textAfterCursor, entireLine);
        
        if (replaceLength > 0) {
            currentCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, replaceLength);
            currentCursor.removeSelectedText();
        }
    }

    if (!subText.contains('\n')) {
        currentCursor.insertText(subText);

        const QString remainingText = text.mid(next);
        if (!remainingText.isEmpty()) {
            QTextCursor newCursor = widget->textCursor();
            const Utils::Text::Position newStart = Utils::Text::Position::fromPositionInDocument(
                newCursor.document(), newCursor.position());
            const Utils::Text::Position
                newEnd{newStart.line, newStart.column + int(remainingText.length())};
            const Utils::Text::Range newRange{newStart, newEnd};
            const QList<Data> newSuggestion{{newRange, newStart, remainingText}};
            widget->insertSuggestion(
                std::make_unique<LLMSuggestion>(newSuggestion, widget->document(), 0));
        }
    } else {
        currentCursor.insertText(subText);

        if (const int seperatorPos = subText.lastIndexOf('\n'); seperatorPos >= 0) {
            const QString newCompletionText = text.mid(startPos + seperatorPos + 1);
            if (!newCompletionText.isEmpty()) {
                const Utils::Text::Position newStart{int(range.begin.line + subText.count('\n')), 0};
                const Utils::Text::Position newEnd{newStart.line, int(newCompletionText.length())};
                const Utils::Text::Range newRange{newStart, newEnd};
                const QList<Data> newSuggestion{{newRange, newEnd, newCompletionText}};
                widget->insertSuggestion(
                    std::make_unique<LLMSuggestion>(newSuggestion, widget->document(), 0));
            }
        }
    }

    return false;
}

bool LLMSuggestion::apply()
{
    const auto &currentSuggestions = suggestions();
    const auto &currentData = currentSuggestions[currentSuggestion()];
    const Utils::Text::Range range = currentData.range;
    const QTextCursor cursor = range.begin.toTextCursor(sourceDocument());
    QString text = currentData.text;

    QTextBlock currentBlock = cursor.block();
    QString textBeforeCursor = currentBlock.text().left(cursor.positionInBlock());
    QString textAfterCursor = currentBlock.text().mid(cursor.positionInBlock());
    QString entireLine = currentBlock.text();

    QTextCursor editCursor = cursor;
    editCursor.beginEditBlock();

    int firstLineEnd = text.indexOf('\n');
    if (firstLineEnd != -1) {
        QString firstLine = text.left(firstLineEnd);
        QString restOfText = text.mid(firstLineEnd);

        int replaceLength = calculateReplaceLength(firstLine, textAfterCursor, entireLine);
        
        if (replaceLength > 0) {
            editCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, replaceLength);
            editCursor.removeSelectedText();
        }
        
        editCursor.insertText(firstLine + restOfText);
    } else {
        int replaceLength = calculateReplaceLength(text, textAfterCursor, entireLine);
        
        if (replaceLength > 0) {
            editCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, replaceLength);
            editCursor.removeSelectedText();
        }
        
        editCursor.insertText(text);
    }

    editCursor.endEditBlock();
    return true;
}

} // namespace QodeAssist

