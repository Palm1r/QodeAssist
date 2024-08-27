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

#include <QTextBlock>

namespace QodeAssist {

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
    QString context;
    for (int i = qMax(0, lineNumber - linesCount); i <= lineNumber; ++i) {
        QString line = getLineText(i, i == lineNumber ? cursorPosition : -1);
        context += line;
        if (i < lineNumber)
            context += "\n";
    }
    return context;
}

QString DocumentContextReader::getContextAfter(int lineNumber,
                                               int cursorPosition,
                                               int linesCount) const
{
    QString context;
    int maxLine = lineNumber + linesCount;
    for (int i = lineNumber; i <= maxLine; ++i) {
        QString line = getLineText(i);
        if (i == lineNumber && cursorPosition >= 0) {
            line = line.mid(cursorPosition);
        }
        context += line;
        if (i < maxLine && !line.isEmpty())
            context += "\n";
    }
    return context;
}

QString DocumentContextReader::readWholeFileBefore(int lineNumber, int cursorPosition) const
{
    QString content;
    QTextBlock block = m_document->begin();
    int currentLine = 0;

    while (block.isValid() && currentLine <= lineNumber) {
        if (currentLine == lineNumber) {
            content += block.text().left(cursorPosition);
            break;
        } else {
            content += block.text() + "\n";
        }
        block = block.next();
        currentLine++;
    }

    return content;
}

QString DocumentContextReader::readWholeFileAfter(int lineNumber, int cursorPosition) const
{
    QString content;
    QTextBlock block = m_document->begin();
    int currentLine = 0;

    while (block.isValid() && currentLine < lineNumber) {
        block = block.next();
        currentLine++;
    }

    while (block.isValid()) {
        if (currentLine == lineNumber) {
            content += block.text().mid(cursorPosition) + "\n";
        } else {
            content += block.text() + "\n";
        }
        block = block.next();
        currentLine++;
    }

    return content.trimmed();
}

} // namespace QodeAssist
