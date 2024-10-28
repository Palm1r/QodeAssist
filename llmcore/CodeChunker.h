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

#include <QObject>
#include <texteditor/textdocument.h>

namespace QodeAssist::LLMCore {

struct CodeChunk
{
    QString content;
    QString filePath;
    int startLine;
    int endLine;
    QString overlapContent;
};

class CodeChunker : public QObject
{
    Q_OBJECT

public:
    static CodeChunker &instance();
    QVector<CodeChunk> splitDocument(TextEditor::TextDocument *document);

private:
    explicit CodeChunker(QObject *parent = nullptr);
    static constexpr int LINES_PER_CHUNK = 50;
    static constexpr int OVERLAP_LINES = 10;
};

} // namespace QodeAssist::LLMCore
