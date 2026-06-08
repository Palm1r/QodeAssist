// Copyright (C) 2025 Povilas Kanapickas
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "IDocumentReader.hpp"

#include <texteditor/textdocument.h>

namespace QodeAssist::Context {

class DocumentReaderQtCreator : public IDocumentReader
{
public:
    DocumentInfo readDocument(const QString &path) const override
    {
        auto *textDocument = TextEditor::TextDocument::textDocumentForFilePath(
            Utils::FilePath::fromString(path));
        if (!textDocument) {
            return {};
        }
        return {
            .document = textDocument->document(),
            .mimeType = textDocument->mimeType(),
            .filePath = path};
    }
};

} // namespace QodeAssist::Context
