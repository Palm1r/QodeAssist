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
    int effectiveStartLine;
    if (m_copyrightInfo.found) {
        effectiveStartLine = qMax(m_copyrightInfo.endLine + 1, lineNumber - linesCount);
    } else {
        effectiveStartLine = qMax(0, lineNumber - linesCount);
    }

    return getContextBetween(effectiveStartLine, lineNumber, cursorPosition);
}

QString DocumentContextReader::getContextAfter(
    int lineNumber, int cursorPosition, int linesCount) const
{
    int endLine = qMin(m_document->blockCount() - 1, lineNumber + linesCount);
    return getContextBetween(lineNumber + 1, endLine, cursorPosition);
}

QString DocumentContextReader::readWholeFileBefore(int lineNumber, int cursorPosition) const
{
    int startLine = 0;
    if (m_copyrightInfo.found) {
        startLine = m_copyrightInfo.endLine + 1;
    }

    startLine = qMin(startLine, lineNumber);

    QString result = getContextBetween(startLine, lineNumber, cursorPosition);

    return result;
}

QString DocumentContextReader::readWholeFileAfter(int lineNumber, int cursorPosition) const
{
    return getContextBetween(lineNumber, m_document->blockCount() - 1, cursorPosition);
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

        if (matchedText.contains("copyright") || matchedText.contains("(C)")
            || matchedText.contains("(c)") || matchedText.contains("©")
            || getYearRegex().match(text).hasMatch() || getNameRegex().match(text).hasMatch()) {
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

QString DocumentContextReader::getContextBetween(int startLine, int endLine, int cursorPosition) const
{
    QString context;
    for (int i = startLine; i <= endLine; ++i) {
        QTextBlock block = m_document->findBlockByNumber(i);
        if (!block.isValid()) {
            break;
        }

        if (i == startLine && cursorPosition >= 0) {
            QString text = block.text();
            if (cursorPosition < text.length()) {
                context += text.mid(cursorPosition);
            }
        } else if (i == endLine && cursorPosition >= 0) {
            context += block.text().left(cursorPosition);
        } else {
            context += block.text();
        }

        if (i < endLine) {
            context += "\n";
        }
    }

    return context;
}

CopyrightInfo DocumentContextReader::copyrightInfo() const
{
    return m_copyrightInfo;
}

LLMCore::ContextData DocumentContextReader::prepareContext(int lineNumber, int cursorPosition) const
{
    QString contextBefore = getContextBefore(lineNumber, cursorPosition);
    QString contextAfter = getContextAfter(lineNumber, cursorPosition);

    QString fileContext;
    fileContext.append("\n ").append(getLanguageAndFileInfo());

    if (Settings::codeCompletionSettings().useProjectChangesCache())
        fileContext.append("\n ").append(
            ChangesManager::instance().getRecentChangesContext(m_textDocument));

    return {.prefix = contextBefore, .suffix = contextAfter, .fileContext = fileContext};
}

QString DocumentContextReader::getContextBefore(int lineNumber, int cursorPosition) const
{
    if (Settings::codeCompletionSettings().readFullFile()) {
        return readWholeFileBefore(lineNumber, cursorPosition);
    } else {
        int effectiveStartLine;
        int beforeCursor = Settings::codeCompletionSettings().readStringsBeforeCursor();
        if (m_copyrightInfo.found) {
            effectiveStartLine = qMax(m_copyrightInfo.endLine + 1, lineNumber - beforeCursor);
        } else {
            effectiveStartLine = qMax(0, lineNumber - beforeCursor);
        }
        return getContextBetween(effectiveStartLine, lineNumber, cursorPosition);
    }
}

QString DocumentContextReader::getContextAfter(int lineNumber, int cursorPosition) const
{
    if (Settings::codeCompletionSettings().readFullFile()) {
        return readWholeFileAfter(lineNumber, cursorPosition);
    } else {
        int endLine = qMin(
            m_document->blockCount() - 1,
            lineNumber + Settings::codeCompletionSettings().readStringsAfterCursor());
        return getContextBetween(lineNumber, endLine, cursorPosition);
    }
}

} // namespace QodeAssist::Context
