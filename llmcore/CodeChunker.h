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
