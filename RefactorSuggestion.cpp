// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RefactorSuggestion.hpp"
#include "LLMSuggestion.hpp"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <texteditor/texteditor.h>
#include <logger/Logger.hpp>

namespace QodeAssist {

namespace {
QString extractLeadingWhitespace(const QString &text)
{
    QString indent;
    int firstLineEnd = text.indexOf('\n');
    QString firstLine = (firstLineEnd != -1) ? text.left(firstLineEnd) : text;
    for (int i = 0; i < firstLine.length(); ++i) {
        if (firstLine[i].isSpace()) {
            indent += firstLine[i];
        } else {
            break;
        }
    }
    return indent;
}
} // anonymous namespace

RefactorSuggestion::RefactorSuggestion(const Data &suggestion, QTextDocument *sourceDocument)
    : TextEditor::TextSuggestion([&suggestion, sourceDocument]() {
        Data expandedData = suggestion;
        
        int startPos = suggestion.range.begin.toPositionInDocument(sourceDocument);
        int endPos = suggestion.range.end.toPositionInDocument(sourceDocument);
        startPos = qBound(0, startPos, sourceDocument->characterCount());
        endPos = qBound(0, endPos, sourceDocument->characterCount());
        
        if (startPos != endPos) {
            QTextCursor startCursor(sourceDocument);
            startCursor.setPosition(startPos);
            int startPosInBlock = startCursor.positionInBlock();
            
            if (startPosInBlock > 0) {
                startCursor.movePosition(QTextCursor::StartOfBlock);
            }
            
            QTextCursor endCursor(sourceDocument);
            endCursor.setPosition(endPos);
            int endPosInBlock = endCursor.positionInBlock();
            
            if (endPosInBlock > 0) {
                endCursor.movePosition(QTextCursor::EndOfBlock);
                if (!endCursor.atEnd()) {
                    endCursor.movePosition(QTextCursor::NextCharacter);
                }
            }
            
            Utils::Text::Position expandedBegin = Utils::Text::Position::fromPositionInDocument(
                sourceDocument, startCursor.position());
            Utils::Text::Position expandedEnd = Utils::Text::Position::fromPositionInDocument(
                sourceDocument, endCursor.position());
            
            expandedData.range = Utils::Text::Range(expandedBegin, expandedEnd);
        }
        
        return expandedData;
    }(), sourceDocument)
    , m_suggestionData(suggestion)
{
    const QString refactoredText = suggestion.text;
    
    int startPos = suggestion.range.begin.toPositionInDocument(sourceDocument);
    int endPos = suggestion.range.end.toPositionInDocument(sourceDocument);
    startPos = qBound(0, startPos, sourceDocument->characterCount());
    endPos = qBound(0, endPos, sourceDocument->characterCount());

    QTextCursor startCursor(sourceDocument);
    startCursor.setPosition(startPos);
    
    if (startPos == endPos) {
        QTextBlock block = startCursor.block();
        QString blockText = block.text();
        int startPosInBlock = startCursor.positionInBlock();
        
        QString leftText = blockText.left(startPosInBlock);
        QString rightText = blockText.mid(startPosInBlock);
        
        QString displayText = leftText + refactoredText + rightText;
        replacementDocument()->setPlainText(displayText);
        
    } else {
        QTextCursor fullLinesCursor(sourceDocument);
        fullLinesCursor.setPosition(startPos);
        fullLinesCursor.movePosition(QTextCursor::StartOfBlock);
        int fullLinesStart = fullLinesCursor.position();
        
        fullLinesCursor.setPosition(endPos);
        fullLinesCursor.movePosition(QTextCursor::EndOfBlock);
        int fullLinesEnd = fullLinesCursor.position();
        
        fullLinesCursor.setPosition(fullLinesStart);
        fullLinesCursor.setPosition(fullLinesEnd, QTextCursor::KeepAnchor);
        QString fullLinesText = fullLinesCursor.selectedText();
        fullLinesText.replace(QChar(0x2029), "\n");
        
        QString oldIndent = extractLeadingWhitespace(fullLinesText);
        QString newIndent = extractLeadingWhitespace(refactoredText);
        
        QString displayText = refactoredText;
        if (newIndent.length() < oldIndent.length()) {
            QString indentDiff = oldIndent.left(oldIndent.length() - newIndent.length());
            QStringList lines = refactoredText.split('\n');
            if (!lines.isEmpty() && !lines[0].trimmed().isEmpty()) {
                lines[0] = indentDiff + lines[0];
                displayText = lines.join('\n');
            }
        }
        
        replacementDocument()->setPlainText(displayText);
    }
}

bool RefactorSuggestion::apply()
{
    const QString text = m_suggestionData.text;
    const Utils::Text::Range range = m_suggestionData.range;

    const QTextCursor startCursor = range.begin.toTextCursor(sourceDocument());
    const QTextCursor endCursor = range.end.toTextCursor(sourceDocument());
    
    const int startPos = startCursor.position();
    const int endPos = endCursor.position();
    
    QTextCursor editCursor(sourceDocument());
    editCursor.beginEditBlock();

    if (startPos == endPos) {
        editCursor.setPosition(startPos);
        editCursor.insertText(text);
    } else {
        editCursor.setPosition(startPos);
        editCursor.setPosition(endPos, QTextCursor::KeepAnchor);
        QString selectedText = editCursor.selectedText();
        selectedText.replace(QChar(0x2029), "\n");
        
        QString oldIndent = extractLeadingWhitespace(selectedText);
        QString newIndent = extractLeadingWhitespace(text);
        
        QString textToInsert = text;
        if (newIndent.length() < oldIndent.length()) {
            QString indentDiff = oldIndent.left(oldIndent.length() - newIndent.length());
            QStringList lines = text.split('\n');
            if (!lines.isEmpty() && !lines[0].trimmed().isEmpty()) {
                lines[0] = indentDiff + lines[0];
                textToInsert = lines.join('\n');
            }
        }
        
        editCursor.setPosition(startPos);
        editCursor.setPosition(endPos, QTextCursor::KeepAnchor);
        editCursor.removeSelectedText();
        editCursor.insertText(textToInsert);
    }

    editCursor.endEditBlock();
    return true;
}

bool RefactorSuggestion::applyWord(TextEditor::TextEditorWidget *widget)
{
    Q_UNUSED(widget)
    return apply();
}

bool RefactorSuggestion::applyLine(TextEditor::TextEditorWidget *widget)
{
    Q_UNUSED(widget)
    return apply();
}

} // namespace QodeAssist

