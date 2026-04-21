// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "context/IDocumentReader.hpp"
#include <memory>
#include <QTextDocument>

namespace QodeAssist {

class MockDocumentReader : public Context::IDocumentReader
{
public:
    MockDocumentReader() = default;

    Context::DocumentInfo readDocument(const QString &path) const override
    {
        return m_documentInfo;
    }

    void setDocumentInfo(const QString &text, const QString &filePath, const QString &mimeType)
    {
        m_document = std::make_unique<QTextDocument>(text);
        m_documentInfo.document = m_document.get();
        m_documentInfo.filePath = filePath;
        m_documentInfo.mimeType = mimeType;
    }

    ~MockDocumentReader() = default;

private:
    Context::DocumentInfo m_documentInfo;
    std::unique_ptr<QTextDocument> m_document;
};

} // namespace QodeAssist
