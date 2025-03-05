/*
 * Copyright (C) 2025 Povilas Kanapickas
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

#include "context/DocumentContextReader.hpp"

#include <gtest/gtest.h>
#include <QTextDocument>

using namespace QodeAssist::Context;

std::ostream &operator<<(std::ostream &out, const QString &value)
{
    out << value.toStdString();
    return out;
}

class DocumentContextReaderTest : public QObject, public testing::Test
{
    Q_OBJECT

protected:
    QTextDocument *createTestDocument(const QString &text)
    {
        auto *doc = new QTextDocument(this);
        doc->setPlainText(text);
        return doc;
    }

    DocumentContextReader createTestReader(const QString &text)
    {
        return DocumentContextReader(createTestDocument(text), "text/python", "/path/to/file");
    }
};

TEST_F(DocumentContextReaderTest, testGetLineText)
{
    auto reader = createTestReader("Line 1\nLine 2\nLine 3");

    EXPECT_EQ(reader.getLineText(0), "Line 1");
    EXPECT_EQ(reader.getLineText(1), "Line 2");
    EXPECT_EQ(reader.getLineText(2), "Line 3");
    EXPECT_EQ(reader.getLineText(0, 4), "Line");
}

TEST_F(DocumentContextReaderTest, testGetContextBefore)
{
    auto reader = createTestReader("Line 1\nLine 2\nLine 3\nLine 4\nLine 5");

    EXPECT_EQ(reader.getContextBefore(2, -1, 2), "Line 1\nLine 2\nLine 3");
    EXPECT_EQ(reader.getContextBefore(4, -1, 3), "Line 2\nLine 3\nLine 4\nLine 5");
}

TEST_F(DocumentContextReaderTest, testGetContextAfter)
{
    auto reader = createTestReader("Line 1\nLine 2\nLine 3\nLine 4\nLine 5");

    EXPECT_EQ(reader.getContextAfter(0, -1, 2), "Line 2\nLine 3");
    EXPECT_EQ(reader.getContextAfter(2, -1, 2), "Line 4\nLine 5");
}

TEST_F(DocumentContextReaderTest, testGetContextBeforeWithCopyright)
{
    auto reader = createTestReader("/* Copyright (C) 2024 */\nLine 1\nLine 2\nLine 3\nLine 4");

    // Test getting context before with copyright header
    EXPECT_EQ(reader.getContextBefore(2, -1, 2), "Line 1\nLine 2");

    // Test getting context before skipping copyright
    EXPECT_EQ(reader.getContextBefore(3, 0, 2), "Line 1\nLine 2\n");
}

TEST_F(DocumentContextReaderTest, testGetContextAfterWithCopyright)
{
    auto reader = createTestReader("/* Copyright (C) 2024 */\nLine 1\nLine 2\nLine 3\nLine 4");

    // Test getting context after copyright header
    EXPECT_EQ(reader.getContextAfter(0, -1, 2), "Line 1\nLine 2");

    // Test getting context after with copyright skipped
    EXPECT_EQ(reader.getContextAfter(1, 0, 2), "Line 2\n");
}

TEST_F(DocumentContextReaderTest, testReadWholeFile)
{
    auto reader = createTestReader("Line 1\nLine 2\nLine 3\nLine 4\nLine 5");

    EXPECT_EQ(reader.readWholeFileBefore(2, -1), "Line 1\nLine 2\nLine 3");
    EXPECT_EQ(reader.readWholeFileAfter(2, -1), "Line 3\nLine 4\nLine 5");
}

TEST_F(DocumentContextReaderTest, testReadWholeFileWithCopyright)
{
    auto reader = createTestReader("/* Copyright (C) 2024 */\nLine 1\nLine 2\nLine 3\nLine 4");

    EXPECT_EQ(reader.readWholeFileBefore(2, -1), "Line 1\nLine 2");
    EXPECT_EQ(reader.readWholeFileAfter(2, -1), "Line 2\nLine 3\nLine 4");

    EXPECT_EQ(reader.readWholeFileBefore(2, 0), "Line 1\n");
    EXPECT_EQ(reader.readWholeFileAfter(2, 0), "Line 2\nLine 3\n");
}

TEST_F(DocumentContextReaderTest, testReadWholeFileWithMultilineCopyright)
{
    auto reader = createTestReader(
        "/*\n * Copyright (C) 2024\n * \n * This file is part of QodeAssist.\n */\n"
        "Line 1\nLine 2");

    EXPECT_EQ(reader.readWholeFileBefore(6, -1), "Line 1\nLine 2");
    EXPECT_EQ(reader.readWholeFileAfter(5, -1), "Line 1\nLine 2");

    EXPECT_EQ(reader.readWholeFileBefore(6, 4), "Line 1\nLine");
    EXPECT_EQ(reader.readWholeFileAfter(5, 4), "Line 1\nLine");
}

TEST_F(DocumentContextReaderTest, testFindCopyrightSingleLine)
{
    auto reader = createTestReader("/* Copyright (C) 2024 */\nCode line 1\nCode line 2");

    auto info = reader.findCopyright();
    ASSERT_TRUE(info.found);
    EXPECT_EQ(info.startLine, 0);
    EXPECT_EQ(info.endLine, 0);
}

TEST_F(DocumentContextReaderTest, testFindCopyrightMultiLine)
{
    auto reader = createTestReader(
        "/*\n * Copyright (C) 2024\n * \n * This file is part of QodeAssist.\n */\nCode line 1");

    auto info = reader.findCopyright();
    ASSERT_TRUE(info.found);
    EXPECT_EQ(info.startLine, 0);
    EXPECT_EQ(info.endLine, 4);
}

TEST_F(DocumentContextReaderTest, testFindCopyrightMultipleBlocks)
{
    auto reader = createTestReader("/* Copyright 2023 */\n\n/* Copyright 2024 */\nCode");

    auto info = reader.findCopyright();
    ASSERT_TRUE(info.found);
    EXPECT_EQ(info.startLine, 0);
    EXPECT_EQ(info.endLine, 0);
}

TEST_F(DocumentContextReaderTest, testFindCopyrightNoCopyright)
{
    auto reader = createTestReader("/* Just a comment */\nCode line 1");

    auto info = reader.findCopyright();
    ASSERT_TRUE(!info.found);
    EXPECT_EQ(info.startLine, -1);
    EXPECT_EQ(info.endLine, -1);
}

TEST_F(DocumentContextReaderTest, testGetContextBetween)
{
    auto reader = createTestReader("Line 1\nLine 2\nLine 3\nLine 4\nLine 5");

    EXPECT_EQ(reader.getContextBetween(1, 3, -1), "Line 2\nLine 3\nLine 4");
    EXPECT_EQ(reader.getContextBetween(0, 2, 4), "Line 1\nLine 2\nLine");
}

#include "DocumentContextReaderTest.moc"
