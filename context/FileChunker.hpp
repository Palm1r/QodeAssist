// FileChunker.hpp
#pragma once

#include <texteditor/textdocument.h>
#include <QDateTime>
#include <QFuture>
#include <QString>

namespace QodeAssist::Context {

struct FileChunk
{
    QString filePath;    // Path to the source file
    int startLine;       // Starting line of the chunk
    int endLine;         // Ending line of the chunk
    QDateTime createdAt; // When the chunk was created
    QDateTime updatedAt; // When the chunk was last updated
    QString content;     // Content of the chunk

    // Helper methods
    int lineCount() const { return endLine - startLine + 1; }
    bool isValid() const { return !filePath.isEmpty() && startLine >= 0 && endLine >= startLine; }
};

class FileChunker : public QObject
{
    Q_OBJECT

public:
    struct ChunkingConfig
    {
        int maxLinesPerChunk = 80;
        int minLinesPerChunk = 40;
        int overlapLines = 20;
        bool skipEmptyLines = true;
        bool preserveFunctions = true;
        bool preserveClasses = true;
        int batchSize = 10;
    };

    explicit FileChunker(QObject *parent = nullptr);
    explicit FileChunker(const ChunkingConfig &config, QObject *parent = nullptr);

    // Main chunking method
    QFuture<QList<FileChunk>> chunkFiles(const QStringList &filePaths);

    // Configuration
    void setConfig(const ChunkingConfig &config);
    ChunkingConfig config() const;

signals:
    void progressUpdated(int processedFiles, int totalFiles);
    void chunkingComplete();
    void error(const QString &errorMessage);

private:
    QList<FileChunk> processFile(const QString &filePath);
    QList<FileChunk> createChunksForDocument(TextEditor::TextDocument *document);
    void processNextBatch(
        std::shared_ptr<QPromise<QList<FileChunk>>> promise,
        const QStringList &files,
        int startIndex);

    ChunkingConfig m_config;
    QString m_error;
};

} // namespace QodeAssist::Context
