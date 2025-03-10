/*
 * Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
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
