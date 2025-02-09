// FileChunker.cpp
#include "FileChunker.hpp"

#include <coreplugin/idocument.h>
#include <texteditor/syntaxhighlighter.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditorconstants.h>

#include <QFutureWatcher>
#include <QTimer>

namespace QodeAssist::Context {

FileChunker::FileChunker(QObject *parent)
    : QObject(parent)
{}

FileChunker::FileChunker(const ChunkingConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{}

QFuture<QList<FileChunk>> FileChunker::chunkFiles(const QStringList &filePaths)
{
    qDebug() << "\nStarting chunking process for" << filePaths.size() << "files";
    qDebug() << "Configuration:"
             << "\n  Max lines per chunk:" << m_config.maxLinesPerChunk
             << "\n  Overlap lines:" << m_config.overlapLines
             << "\n  Skip empty lines:" << m_config.skipEmptyLines
             << "\n  Preserve functions:" << m_config.preserveFunctions
             << "\n  Preserve classes:" << m_config.preserveClasses
             << "\n  Batch size:" << m_config.batchSize;

    auto promise = std::make_shared<QPromise<QList<FileChunk>>>();
    promise->start();

    if (filePaths.isEmpty()) {
        qDebug() << "No files to process";
        promise->addResult({});
        promise->finish();
        return promise->future();
    }

    processNextBatch(promise, filePaths, 0);
    return promise->future();
}

void FileChunker::processNextBatch(
    std::shared_ptr<QPromise<QList<FileChunk>>> promise, const QStringList &files, int startIndex)
{
    if (startIndex >= files.size()) {
        emit chunkingComplete();
        promise->finish();
        return;
    }

    int endIndex = qMin(startIndex + m_config.batchSize, files.size());
    QList<FileChunk> batchChunks;

    for (int i = startIndex; i < endIndex; ++i) {
        try {
            auto chunks = processFile(files[i]);
            batchChunks.append(chunks);
        } catch (const std::exception &e) {
            emit error(QString("Error processing file %1: %2").arg(files[i], e.what()));
        }
        emit progressUpdated(i + 1, files.size());
    }

    promise->addResult(batchChunks);

    // Планируем обработку следующего батча
    QTimer::singleShot(0, this, [this, promise, files, endIndex]() {
        processNextBatch(promise, files, endIndex);
    });
}

QList<FileChunk> FileChunker::processFile(const QString &filePath)
{
    qDebug() << "\nProcessing file:" << filePath;

    auto document = new TextEditor::TextDocument;
    auto filePathObj = Utils::FilePath::fromString(filePath);
    auto result = document->open(&m_error, filePathObj, filePathObj);
    if (result != Core::IDocument::OpenResult::Success) {
        qDebug() << "Failed to open document:" << filePath << "-" << m_error;
        emit error(QString("Failed to open document: %1 - %2").arg(filePath, m_error));
        delete document;
        return {};
    }

    qDebug() << "Document opened successfully. Line count:" << document->document()->blockCount();

    auto chunks = createChunksForDocument(document);
    qDebug() << "Created" << chunks.size() << "chunks for file";

    delete document;
    return chunks;
}

QList<FileChunk> FileChunker::createChunksForDocument(TextEditor::TextDocument *document)
{
    QList<FileChunk> chunks;
    QString filePath = document->filePath().toString();
    qDebug() << "\nCreating chunks for document:" << filePath << "\nConfiguration:"
             << "\n  Max lines per chunk:" << m_config.maxLinesPerChunk
             << "\n  Min lines per chunk:" << m_config.minLinesPerChunk
             << "\n  Overlap lines:" << m_config.overlapLines;
    // Если файл меньше минимального размера чанка, создаем один чанк
    if (document->document()->blockCount() <= m_config.minLinesPerChunk) {
        FileChunk chunk;
        chunk.filePath = filePath;
        chunk.startLine = 0;
        chunk.endLine = document->document()->blockCount() - 1;
        chunk.createdAt = QDateTime::currentDateTime();
        chunk.updatedAt = chunk.createdAt;

        QString content;
        QTextBlock block = document->document()->firstBlock();
        while (block.isValid()) {
            content += block.text() + "\n";
            block = block.next();
        }
        chunk.content = content;

        qDebug() << "File is smaller than minimum chunk size. Creating single chunk:"
                 << "\n  Lines:" << chunk.lineCount() << "\n  Content size:" << chunk.content.size()
                 << "bytes";

        chunks.append(chunk);
        return chunks;
    }

    // Для больших файлов создаем чанки фиксированного размера с перекрытием
    int currentStartLine = 0;
    int lineCount = 0;
    QString content;
    QTextBlock block = document->document()->firstBlock();

    while (block.isValid()) {
        content += block.text() + "\n";
        lineCount++;

        // Если достигли размера чанка или это последний блок
        if (lineCount >= m_config.maxLinesPerChunk || !block.next().isValid()) {
            FileChunk chunk;
            chunk.filePath = filePath;
            chunk.startLine = currentStartLine;
            chunk.endLine = currentStartLine + lineCount - 1;
            chunk.content = content;
            chunk.createdAt = QDateTime::currentDateTime();
            chunk.updatedAt = chunk.createdAt;

            qDebug() << "Creating chunk:"
                     << "\n  Start line:" << chunk.startLine << "\n  End line:" << chunk.endLine
                     << "\n  Lines:" << chunk.lineCount()
                     << "\n  Content size:" << chunk.content.size() << "bytes";

            chunks.append(chunk);

            // Начинаем новый чанк с учетом перекрытия
            if (block.next().isValid()) {
                // Отступаем назад на размер перекрытия
                int overlapLines = qMin(m_config.overlapLines, lineCount);
                currentStartLine = chunk.endLine - overlapLines + 1;

                // Сбрасываем контент, но добавляем перекрывающиеся строки
                content.clear();
                QTextBlock overlapBlock = document->document()->findBlockByLineNumber(
                    currentStartLine);
                while (overlapBlock.isValid() && overlapBlock.blockNumber() <= chunk.endLine) {
                    content += overlapBlock.text() + "\n";
                    overlapBlock = overlapBlock.next();
                }
                lineCount = overlapLines;
            }
        }

        block = block.next();
    }

    qDebug() << "Finished creating chunks for file:" << filePath
             << "\nTotal chunks:" << chunks.size();

    return chunks;
}

void FileChunker::setConfig(const ChunkingConfig &config)
{
    m_config = config;
}

FileChunker::ChunkingConfig FileChunker::config() const
{
    return m_config;
}

} // namespace QodeAssist::Context
