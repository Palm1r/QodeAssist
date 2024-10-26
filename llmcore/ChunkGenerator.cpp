#include "ChunkGenerator.h"

#include "Logger.hpp"

namespace QodeAssist::LLMCore {

CodeChunker &CodeChunker::instance()
{
    static CodeChunker instance;
    return instance;
}

CodeChunker::CodeChunker(QObject *parent)
    : QObject(parent)
{}

QVector<CodeChunk> CodeChunker::splitDocument(TextEditor::TextDocument *document)
{
    QVector<CodeChunk> chunks;
    if (!document || !document->document()) {
        qDebug() << "Invalid document";
        return chunks;
    }

    QTextDocument *textDocument = document->document();
    QTextBlock block = textDocument->begin();
    QString filePath = document->filePath().toString();

    qDebug() << "\nProcessing document:" << filePath;
    qDebug() << "Total blocks:" << textDocument->blockCount();

    QString currentChunk;
    int startLine = 0;
    int currentLine = 0;
    int linesInCurrentChunk = 0;

    while (block.isValid()) {
        QString line = block.text();

        if (linesInCurrentChunk >= LINES_PER_CHUNK && !currentChunk.isEmpty()) {
            CodeChunk chunk;
            chunk.content = currentChunk;
            chunk.filePath = filePath;
            chunk.startLine = startLine;
            chunk.endLine = currentLine - 1;

            QTextBlock overlapBlock = block;
            QString overlap;
            int overlapLines = 0;

            while (overlapBlock.isValid() && overlapLines < OVERLAP_LINES) {
                overlap += overlapBlock.text() + "\n";
                overlapBlock = overlapBlock.next();
                overlapLines++;
            }
            chunk.overlapContent = overlap;

            chunks.append(chunk);

            qDebug() << "Created chunk:" << chunks.size() << "lines:" << chunk.startLine << "-"
                     << chunk.endLine << "lines count:" << linesInCurrentChunk
                     << "overlap lines:" << overlapLines;

            for (int i = 0; i < OVERLAP_LINES && block.isValid(); i++) {
                block = block.previous();
                currentLine--;
            }

            currentChunk.clear();
            linesInCurrentChunk = 0;
            startLine = currentLine;
            continue;
        }

        currentChunk += line + "\n";
        linesInCurrentChunk++;

        block = block.next();
        currentLine++;
    }

    if (!currentChunk.isEmpty()) {
        CodeChunk chunk;
        chunk.content = currentChunk;
        chunk.filePath = filePath;
        chunk.startLine = startLine;
        chunk.endLine = currentLine - 1;

        chunks.append(chunk);

        qDebug() << "Created final chunk:" << chunks.size() << "lines:" << chunk.startLine << "-"
                 << chunk.endLine << "lines count:" << linesInCurrentChunk << "overlap lines:" << 0;
    }

    qDebug() << "Total chunks created:" << chunks.size() << "\n";

    for (int i = 0; i < chunks.size(); ++i) {
        const auto &chunk = chunks[i];
        qDebug() << "Chunk" << i + 1 << ":"
                 << "lines" << chunk.startLine << "-" << chunk.endLine
                 << "main content size:" << chunk.content.length()
                 << "overlap content size:" << chunk.overlapContent.length() << "content"
                 << chunk.overlapContent;
    }

    return chunks;
}

} // namespace QodeAssist::LLMCore
