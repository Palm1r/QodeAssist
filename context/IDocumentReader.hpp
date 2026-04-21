// Copyright (C) 2025 Povilas Kanapickas
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QTextDocument>

namespace QodeAssist::Context {

struct DocumentInfo
{
    QTextDocument *document = nullptr; // not owned
    QString mimeType;
    QString filePath;
};

class IDocumentReader
{
public:
    virtual ~IDocumentReader() = default;

    virtual DocumentInfo readDocument(const QString &path) const = 0;
};

} // namespace QodeAssist::Context
