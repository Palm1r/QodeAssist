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

#include "DocumentContextReader.hpp"

#include <languageserverprotocol/lsptypes.h>
#include <QFileInfo>
#include <QTextBlock>

#include "CodeCompletionSettings.hpp"

#include "ChangesManager.h"

const QRegularExpression &getYearRegex()
{
    static const QRegularExpression yearRegex("\\b(19|20)\\d{2}\\b");
    return yearRegex;
}

const QRegularExpression &getNameRegex()
{
    static const QRegularExpression nameRegex("\\b[A-Z][a-z.]+ [A-Z][a-z.]+\\b");
    return nameRegex;
}

const QRegularExpression &getCommentRegex()
{
    static const QRegularExpression commentRegex(
        R"((/\*[\s\S]*?\*/|//.*$|#.*$|//{2,}[\s\S]*?//{2,}))", QRegularExpression::MultilineOption);
    return commentRegex;
}

namespace QodeAssist::Context {

DocumentContextReader::DocumentContextReader(
    QTextDocument *document, const QString &mimeType, const QString &filePath)
    : m_document(document)
    , m_mimeType(mimeType)
    , m_filePath(filePath)
{
    m_copyrightInfo = findCopyright();
}

QString DocumentContextReader::getLineText(int lineNumber, int cursorPosition) const
{
    if (!m_document || lineNumber < 0)
        return QString();

    QTextBlock block = m_document->begin();
    int currentLine = 0;

    while (block.isValid()) {
        if (currentLine == lineNumber) {
            QString text = block.text();
            if (cursorPosition >= 0 && cursorPosition <= text.length()) {
                text = text.left(cursorPosition);
            }
            return text;
        }

        block = block.next();
        currentLine++;
    }

    return QString();
}

QString DocumentContextReader::getContextBefore(
    int lineNumber, int cursorPosition, int linesCount) const
{
    int startLine = lineNumber - linesCount + 1;
    if (m_copyrightInfo.found) {
        startLine = qMax(m_copyrightInfo.endLine + 1, startLine);
    }

    return getContextBetween(startLine, -1, lineNumber, cursorPosition);
}

QString DocumentContextReader::getContextAfter(
    int lineNumber, int cursorPosition, int linesCount) const
{
    int endLine = lineNumber + linesCount - 1;
    if (m_copyrightInfo.found && m_copyrightInfo.endLine >= lineNumber) {
        lineNumber = m_copyrightInfo.endLine + 1;
        cursorPosition = -1;
    }
    return getContextBetween(lineNumber, cursorPosition, endLine, -1);
}

QString DocumentContextReader::readWholeFileBefore(int lineNumber, int cursorPosition) const
{
    int startLine = 0;
    if (m_copyrightInfo.found) {
        startLine = m_copyrightInfo.endLine + 1;
    }

    return getContextBetween(startLine, -1, lineNumber, cursorPosition);
}

QString DocumentContextReader::readWholeFileAfter(int lineNumber, int cursorPosition) const
{
    int endLine = m_document->blockCount() - 1;
    if (m_copyrightInfo.found && m_copyrightInfo.endLine >= lineNumber) {
        lineNumber = m_copyrightInfo.endLine + 1;
        cursorPosition = -1;
    }
    return getContextBetween(lineNumber, cursorPosition, endLine, -1);
}

QString DocumentContextReader::getLanguageAndFileInfo() const
{
    QString language = LanguageServerProtocol::TextDocumentItem::mimeTypeToLanguageId(m_mimeType);
    QString fileExtension = QFileInfo(m_filePath).suffix();

    return QString("Language: %1 (MIME: %2) filepath: %3(%4)\n\n")
        .arg(language, m_mimeType, m_filePath, fileExtension);
}

CopyrightInfo DocumentContextReader::findCopyright()
{
    CopyrightInfo result = {-1, -1, false};

    QString text = m_document->toPlainText();
    QRegularExpressionMatchIterator matchIterator = getCommentRegex().globalMatch(text);

    QList<CopyrightInfo> copyrightBlocks;

    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        QString matchedText = match.captured().toLower();

        bool hasCopyrightIndicator = matchedText.contains("copyright")
                                     || matchedText.contains("(c)") || matchedText.contains("©")
                                     || matchedText.contains("copr.")
                                     || matchedText.contains("all rights reserved")
                                     || matchedText.contains("proprietary")
                                     || matchedText.contains("licensed under")
                                     || matchedText.contains("license:")
                                     || matchedText.contains("gpl") || matchedText.contains("lgpl")
                                     || matchedText.contains("mit license")
                                     || matchedText.contains("apache license")
                                     || matchedText.contains("bsd license")
                                     || matchedText.contains("mozilla public license")
                                     || matchedText.contains("copyleft");

        bool hasYear = getYearRegex().match(matchedText).hasMatch();
        bool hasName = getNameRegex().match(matchedText).hasMatch();

        if ((hasCopyrightIndicator && (hasYear || hasName)) || (hasYear && hasName)) {
            int startPos = match.capturedStart();
            int endPos = match.capturedEnd();

            CopyrightInfo info;
            info.startLine = m_document->findBlock(startPos).blockNumber();
            info.endLine = m_document->findBlock(endPos).blockNumber();
            info.found = true;

            copyrightBlocks.append(info);
        }
    }

    for (int i = 0; i < copyrightBlocks.size() - 1; ++i) {
        if (copyrightBlocks[i].endLine + 1 >= copyrightBlocks[i + 1].startLine) {
            copyrightBlocks[i].endLine = copyrightBlocks[i + 1].endLine;
            copyrightBlocks.removeAt(i + 1);
            --i;
        }
    }

    if (!copyrightBlocks.isEmpty()) { // temproary solution, need cache
        return copyrightBlocks.first();
    }

    return result;
}

QString DocumentContextReader::getContextBetween(
    int startLine, int startCursorPosition, int endLine, int endCursorPosition) const
{
    QString context;

    startLine = qMax(startLine, 0);
    endLine = qMin(endLine, m_document->blockCount() - 1);

    if (startLine > endLine) {
        return context;
    }

    if (startLine == endLine) {
        auto block = m_document->findBlockByNumber(startLine);
        if (!block.isValid()) {
            return context;
        }

        auto text = block.text();

        if (startCursorPosition < 0) {
            startCursorPosition = 0;
        }
        if (endCursorPosition < 0) {
            endCursorPosition = text.size();
        }

        if (startCursorPosition >= endCursorPosition) {
            return context;
        }

        return text.mid(startCursorPosition, endCursorPosition - startCursorPosition);
    }

    // first line
    {
        auto block = m_document->findBlockByNumber(startLine);
        if (!block.isValid()) {
            return context;
        }
        auto text = block.text();
        if (startCursorPosition < 0) {
            context += text + "\n";
        } else {
            context += text.right(text.size() - startCursorPosition) + "\n";
        }
    }

    // intermediate lines, if any
    for (int i = startLine + 1; i <= endLine - 1; ++i) {
        auto block = m_document->findBlockByNumber(i);
        if (!block.isValid()) {
            return context;
        }
        context += block.text() + "\n";
    }

    // last line
    {
        auto block = m_document->findBlockByNumber(endLine);
        if (!block.isValid()) {
            return context;
        }
        auto text = block.text();
        if (endCursorPosition < 0) {
            context += text;
        } else {
            context += text.left(endCursorPosition);
        }
    }

    return context;
}

CopyrightInfo DocumentContextReader::copyrightInfo() const
{
    return m_copyrightInfo;
}

LLMCore::ContextData DocumentContextReader::prepareContext(
    int lineNumber, int cursorPosition, const Settings::CodeCompletionSettings &settings) const
{
    QString contextBefore;
    QString contextAfter;
    if (settings.readFullFile()) {
        contextBefore = readWholeFileBefore(lineNumber, cursorPosition);
        contextAfter = readWholeFileAfter(lineNumber, cursorPosition);
    } else {
        // Note that readStrings{After,Before}Cursor include current line, but linesCount argument of
        // getContext{After,Before} do not
        contextBefore
            = getContextBefore(lineNumber, cursorPosition, settings.readStringsBeforeCursor() + 1);
        contextAfter
            = getContextAfter(lineNumber, cursorPosition, settings.readStringsAfterCursor() + 1);
    }

    QString fileContext;
    fileContext.append("\n ").append(getLanguageAndFileInfo());

    if (settings.useProjectChangesCache())
        fileContext.append("Recent Project Changes Context:\n ")
            .append(ChangesManager::instance().getRecentChangesContext(m_textDocument));

    return {.prefix = contextBefore, .suffix = contextAfter, .fileContext = fileContext};
}

} // namespace QodeAssist::Context
