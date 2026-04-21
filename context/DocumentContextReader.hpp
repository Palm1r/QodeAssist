// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <texteditor/textdocument.h>
#include <QTextDocument>

#include <pluginllmcore/ContextData.hpp>
#include <settings/CodeCompletionSettings.hpp>

namespace QodeAssist::Context {

struct CopyrightInfo
{
    int startLine;
    int endLine;
    bool found;
};

class DocumentContextReader
{
public:
    DocumentContextReader(
        QTextDocument *m_document, const QString &mimeType, const QString &filePath);

    QString getLineText(int lineNumber, int cursorPosition = -1) const;

    /**
     * @brief Retrieves @c linesCount lines of context ending at @c lineNumber at
     * @c cursorPosition in that line. The line at @c lineNumber is inclusive regardless of
     * @c cursorPosition.
     */
    QString getContextBefore(int lineNumber, int cursorPosition, int linesCount) const;

    /**
     * @brief Retrieves @c linesCount lines of context starting at @c lineNumber at
     * @c cursorPosition in that line. The line at @c lineNumber is inclusive regardless of
     * @c cursorPosition.
     */
    QString getContextAfter(int lineNumber, int cursorPosition, int linesCount) const;

    /**
     * @brief Retrieves whole file ending at @c lineNumber at @c cursorPosition in that line.
     */
    QString readWholeFileBefore(int lineNumber, int cursorPosition) const;

    /**
     * @brief Retrieves whole file starting at @c lineNumber at @c cursorPosition in that line.
     */
    QString readWholeFileAfter(int lineNumber, int cursorPosition) const;

    QString getLanguageAndFileInfo() const;
    CopyrightInfo findCopyright();
    QString getContextBetween(
        int startLine, int startCursorPosition, int endLine, int endCursorPosition) const;

    CopyrightInfo copyrightInfo() const;

    PluginLLMCore::ContextData prepareContext(
        int lineNumber, int cursorPosition, const Settings::CodeCompletionSettings &settings) const;

private:
    TextEditor::TextDocument *m_textDocument;
    QTextDocument *m_document;
    QString m_mimeType;
    QString m_filePath;

    // Used to omit copyright headers from context. If context would otherwise include copyright
    // header it is excluded by deleting it from the returned context. This means, that the
    // returned context may contain less information than requested. If the cursor is within copyright
    // header, then the context may be empty if the context window is small.
    CopyrightInfo m_copyrightInfo;
};

} // namespace QodeAssist::Context
