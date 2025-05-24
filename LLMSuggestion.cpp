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

QString mergeWithRightText(const QString &suggestion, const QString &rightText)
{
    if (suggestion.isEmpty() || rightText.isEmpty()) {
        return suggestion;
    }

    int j = 0;
    QString processed = rightText;
    QSet<int> matchedPositions;

    for (int i = 0; i < suggestion.length() && j < processed.length(); ++i) {
        if (suggestion[i] == processed[j]) {
            matchedPositions.insert(j);
            ++j;
        }
    }

    if (matchedPositions.isEmpty()) {
        return suggestion + rightText;
    }

    QList<int> positions = matchedPositions.values();
    std::sort(positions.begin(), positions.end(), std::greater<int>());
    for (int pos : positions) {
        processed.remove(pos, 1);
    }

    return suggestion;
}

static int levenshteinDistance(const QString &source, const QString &target)
{
    if (source == target) {
        return 0;
    }

    int sourceCount = source.size();
    int targetCount = target.size();

    if (sourceCount == 0) {
        return targetCount;
    }

    if (targetCount == 0) {
        return sourceCount;
    }

    if (sourceCount > targetCount) {
        return levenshteinDistance(target, source);
    }

    QVector<int> previousColumn;
    previousColumn.reserve(targetCount + 1);
    for (int i = 0; i < targetCount + 1; ++i) {
        previousColumn.append(i);
    }

    QVector<int> column(targetCount + 1, 0);
    for (int i = 0; i < sourceCount; ++i) {
        column[0] = i + 1;
        for (int j = 0; j < targetCount; ++j) {
            column[j + 1] = std::min(
                {1 + column.at(j),
                 1 + previousColumn.at(1 + j),
                 previousColumn.at(j) + ((source.at(i) == target.at(j)) ? 0 : 1)});
        }
        column.swap(previousColumn);
    }

    return previousColumn.at(targetCount);
}

static int linesToReplace(const QString &suggestion, const QTextCursor &cursor)
{
    int suggestedLineCount = 0;
    for (int idx = suggestion.indexOf('\n'); idx != -1; idx = suggestion.indexOf('\n', idx + 1)) {
        ++suggestedLineCount;
    }
    if (suggestedLineCount == 0) {
        return 0;
    }

    QTextCursor lineReadCursor = cursor;
    lineReadCursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    int minDifference = 1e6;
    int bestLineCount = 0;

    for (int i = 0; i < suggestedLineCount; ++i) {
        lineReadCursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
        lineReadCursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        QString curText = lineReadCursor.selectedText();

        int difference = levenshteinDistance(curText, suggestion);
        if (difference > minDifference) {
            continue;
        }

        bestLineCount = i + 1;
        minDifference = difference;
    }

    double minSimilarity = 0.4; // configurable?
    int threshold = suggestion.length() * minSimilarity;

    return minDifference < threshold ? bestLineCount : 0;
}

static QString existingTailToKeep(const QString &suggestedLine, const QString &existingLine)
{
    int minDifference = 1e6;
    int bestLength = 0;

    for (int i = 0; i <= existingLine.length(); ++i) {
        int difference = levenshteinDistance(suggestedLine, existingLine.left(i));
        if (difference > minDifference) {
            continue;
        }

        minDifference = difference;
        bestLength = i;
    }
    return existingLine.mid(bestLength);
}

LLMSuggestion::LLMSuggestion(
    const QList<Data> &suggestions, QTextDocument *sourceDocument, int currentCompletion)
    : TextEditor::CyclicSuggestion(suggestions, sourceDocument, currentCompletion)
{
    const auto &data = suggestions[currentCompletion];

    int startPos = data.range.begin.toPositionInDocument(sourceDocument);
    int endPos = data.range.end.toPositionInDocument(sourceDocument);

    startPos = qBound(0, startPos, sourceDocument->characterCount());
    endPos = qBound(startPos, endPos, sourceDocument->characterCount());

    QTextCursor cursor(sourceDocument);
    cursor.setPosition(startPos);
    QTextBlock block = cursor.block();
    QString blockText = block.text();

    int cursorPositionInBlock = cursor.positionInBlock();

    QString rightText = blockText.mid(cursorPositionInBlock);

    if (!data.text.contains('\n')) {
        QString processedRightText = mergeWithRightText(data.text, rightText);
        processedRightText = processedRightText.mid(data.text.length());
        QString displayText = blockText.left(cursorPositionInBlock) + data.text
                              + processedRightText;
        replacementDocument()->setPlainText(displayText);
    } else {
        QString displayText = blockText.left(cursorPositionInBlock) + data.text;
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
    const Utils::Text::Range range = suggestions()[currentSuggestion()].range;
    const QTextCursor cursor = range.begin.toTextCursor(sourceDocument());
    QTextCursor currentCursor = widget->textCursor();
    const QString text = suggestions()[currentSuggestion()].text;

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

    QTextBlock currentBlock = currentCursor.block();
    QString textAfterCursor = currentBlock.text().mid(currentCursor.positionInBlock());

    if (!subText.contains('\n')) {
        QTextCursor deleteCursor = currentCursor;
        deleteCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        deleteCursor.removeSelectedText();

        QString mergedText = mergeWithRightText(subText, textAfterCursor);
        currentCursor.insertText(mergedText);
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
    const Utils::Text::Range range = suggestions()[currentSuggestion()].range;
    const QTextCursor cursor = range.begin.toTextCursor(sourceDocument());
    const QString text = suggestions()[currentSuggestion()].text;

    QTextBlock currentBlock = cursor.block();
    QString textAfterCursor = currentBlock.text().mid(cursor.positionInBlock());

    QTextCursor editCursor = cursor;

    int firstLineEnd = text.indexOf('\n');
    if (firstLineEnd != -1) {
        int toReplace = linesToReplace(text, cursor);

        if (toReplace == 0) {
            editCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            editCursor.removeSelectedText();

            QString firstLine = text.left(firstLineEnd);
            QString mergedFirstLine = mergeWithRightText(firstLine, textAfterCursor);
            QString restOfText = text.mid(firstLineEnd);
            editCursor.insertText(mergedFirstLine + restOfText);
        } else {
            editCursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, toReplace);
            editCursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);

            QTextCursor lastLineCursor = editCursor;
            lastLineCursor.select(QTextCursor::LineUnderCursor);
            QString lastExistingLine = lastLineCursor.selectedText();

            QString lastSuggestedLine = text.mid(text.lastIndexOf('\n') + 1);

            QString tail = existingTailToKeep(lastSuggestedLine, lastExistingLine);
            editCursor.insertText(text + tail);
        }
    } else {
        editCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        editCursor.removeSelectedText();

        QString mergedText = mergeWithRightText(text, textAfterCursor);
        editCursor.insertText(mergedText);
    }

    return true;
}

} // namespace QodeAssist
