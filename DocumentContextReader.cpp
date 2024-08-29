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

#include <QFileInfo>
#include <QTextBlock>
#include <languageserverprotocol/lsptypes.h>

#include "QodeAssistSettings.hpp"

namespace QodeAssist {

DocumentContextReader::DocumentContextReader(TextEditor::TextDocument *textDocument)
    : m_textDocument(textDocument)
    , m_document(textDocument->document())
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

QString DocumentContextReader::getContextBefore(int lineNumber,
                                                int cursorPosition,
                                                int linesCount) const
{
    int effectiveStartLine;
    if (m_copyrightInfo.found) {
        effectiveStartLine = qMax(m_copyrightInfo.endLine + 1, lineNumber - linesCount);
    } else {
        effectiveStartLine = qMax(0, lineNumber - linesCount);
    }

    return getContextBetween(effectiveStartLine, lineNumber, cursorPosition);
}

QString DocumentContextReader::getContextAfter(int lineNumber,
                                               int cursorPosition,
                                               int linesCount) const
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
    if (!m_textDocument)
        return QString();

    QString language = LanguageServerProtocol::TextDocumentItem::mimeTypeToLanguageId(
        m_textDocument->mimeType());
    QString mimeType = m_textDocument->mimeType();
    QString filePath = m_textDocument->filePath().toString();
    QString fileExtension = QFileInfo(filePath).suffix();

    return QString("//Language: %1 (MIME: %2) filepath: %3(%4)\n\n")
        .arg(language, mimeType, filePath, fileExtension);
}

QString DocumentContextReader::getSpecificInstructions() const
{
    QString specificInstruction = settings().specificInstractions().arg(
        LanguageServerProtocol::TextDocumentItem::mimeTypeToLanguageId(m_textDocument->mimeType()));
    return QString("//Instructions: %1").arg(specificInstruction);
}

CopyrightInfo DocumentContextReader::findCopyright()
{
    CopyrightInfo result = {-1, -1, false};

    QString text = m_document->toPlainText();
    QRegularExpressionMatchIterator matchIterator = getCopyrightRegex().globalMatch(text);

    QList<CopyrightInfo> copyrightBlocks;

    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        int startPos = match.capturedStart();
        int endPos = match.capturedEnd();

        CopyrightInfo info;
        info.startLine = m_document->findBlock(startPos).blockNumber();
        info.endLine = m_document->findBlock(endPos).blockNumber();
        info.found = true;

        copyrightBlocks.append(info);
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

QString DocumentContextReader::getContextBetween(int startLine,
                                                 int endLine,
                                                 int cursorPosition) const
{
    QString context;
    for (int i = startLine; i <= endLine; ++i) {
        QTextBlock block = m_document->findBlockByNumber(i);
        if (!block.isValid()) {
            break;
        }
        if (i == endLine) {
            context += block.text().left(cursorPosition);
        } else {
            context += block.text() + "\n";
        }
    }

    return context;
}

const QRegularExpression &DocumentContextReader::getCopyrightRegex()
{
    static const QRegularExpression copyrightRegex(
        R"((?:/\*[\s\S]*?Copyright[\s\S]*?\*/|  // Copyright[\s\S]*?(?:\n\s*//.*)*|///.*Copyright[\s\S]*?(?:\n\s*///.*)*)|(?://))",
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    return copyrightRegex;
}

} // namespace QodeAssist
