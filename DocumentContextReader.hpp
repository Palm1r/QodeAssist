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

#include <QTextDocument>

namespace QodeAssist {

class DocumentContextReader
{
public:
    DocumentContextReader(QTextDocument *doc)
        : m_document(doc)
    {}

    QString getLineText(int lineNumber, int cursorPosition = -1) const;
    QString getContextBefore(int lineNumber, int cursorPosition, int linesCount) const;
    QString getContextAfter(int lineNumber, int cursorPosition, int linesCount) const;
    QString readWholeFileBefore(int lineNumber, int cursorPosition) const;
    QString readWholeFileAfter(int lineNumber, int cursorPosition) const;

private:
    QTextDocument *m_document;
};

} // namespace QodeAssist
