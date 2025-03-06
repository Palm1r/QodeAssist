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
#include <QSharedPointer>
#include <QTextDocument>

using namespace QodeAssist::Context;
using namespace QodeAssist::LLMCore;
using namespace QodeAssist::Settings;

QT_BEGIN_NAMESPACE

// gtest can't pick pretty printer when comparing QString
inline void PrintTo(const QString &value, ::std::ostream *out)
{
    *out << '"' << value.toStdString() << '"';
}

QT_END_NAMESPACE

std::ostream &operator<<(std::ostream &out, const QString &value)
{
    out << '"' << value.toStdString() << '"';
    return out;
}

template<class T>
std::ostream &operator<<(std::ostream &out, const QVector<T> &value)
{
    out << "[";
    for (const auto &el : value) {
        out << value << ", ";
    }
    out << "]";
    return out;
}

template<class T>
std::ostream &operator<<(std::ostream &out, const std::optional<T> &value)
{
    if (value.has_value()) {
        out << value.value();
    } else {
        out << "(no value)";
    }
    return out;
}

namespace QodeAssist::LLMCore {
std::ostream &operator<<(std::ostream &out, const Message &value)
{
    out << "Message{"
        << "role=" << value.role << "content=" << value.content << "}";
    return out;
}

std::ostream &operator<<(std::ostream &out, const ContextData &value)
{
    out << "ContextData{"
        << "\n  systemPrompt=" << value.systemPrompt << "\n  prefix=" << value.prefix
        << "\n  suffix=" << value.suffix << "\n  fileContext=" << value.fileContext
        << "\n  history=" << value.history << "\n}";
    return out;
}
} // namespace QodeAssist::LLMCore

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

    QSharedPointer<CodeCompletionSettings> createSettingsForWholeFile()
    {
        // CodeCompletionSettings is noncopyable
        auto settings = QSharedPointer<CodeCompletionSettings>::create();
        settings->readFullFile.setValue(true);
        return settings;
    }

    QSharedPointer<CodeCompletionSettings> createSettingsForLines(int linesBefore, int linesAfter)
    {
        // CodeCompletionSettings is noncopyable
        auto settings = QSharedPointer<CodeCompletionSettings>::create();
        settings->readFullFile.setValue(false);
        settings->readStringsBeforeCursor.setValue(linesBefore);
        settings->readStringsAfterCursor.setValue(linesAfter);
        return settings;
    }
};

TEST_F(DocumentContextReaderTest, testGetLineText)
{
    auto reader = createTestReader("Line 0\nLine 1\nLine 2");

    EXPECT_EQ(reader.getLineText(0), "Line 0");
    EXPECT_EQ(reader.getLineText(1), "Line 1");
    EXPECT_EQ(reader.getLineText(2), "Line 2");
    EXPECT_EQ(reader.getLineText(0, 4), "Line");
}

TEST_F(DocumentContextReaderTest, testGetContextBefore)
{
    auto reader = createTestReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    EXPECT_EQ(reader.getContextBefore(0, -1, 2), "Line 0");
    EXPECT_EQ(reader.getContextBefore(1, -1, 2), "Line 0\nLine 1");
    EXPECT_EQ(reader.getContextBefore(2, -1, 2), "Line 1\nLine 2");
    EXPECT_EQ(reader.getContextBefore(3, -1, 2), "Line 2\nLine 3");
    EXPECT_EQ(reader.getContextBefore(0, 1, 2), "L");
    EXPECT_EQ(reader.getContextBefore(1, 1, 2), "Line 0\nL");
    EXPECT_EQ(reader.getContextBefore(2, 1, 2), "Line 1\nL");
    EXPECT_EQ(reader.getContextBefore(3, 1, 2), "Line 2\nL");
}

TEST_F(DocumentContextReaderTest, testGetContextAfter)
{
    auto reader = createTestReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    EXPECT_EQ(reader.getContextAfter(0, -1, 2), "Line 0\nLine 1");
    EXPECT_EQ(reader.getContextAfter(1, -1, 2), "Line 1\nLine 2");
    EXPECT_EQ(reader.getContextAfter(2, -1, 2), "Line 2\nLine 3");
    EXPECT_EQ(reader.getContextAfter(3, -1, 2), "Line 3\nLine 4");
    EXPECT_EQ(reader.getContextAfter(0, 1, 2), "ine 0\nLine 1");
    EXPECT_EQ(reader.getContextAfter(1, 1, 2), "ine 1\nLine 2");
    EXPECT_EQ(reader.getContextAfter(2, 1, 2), "ine 2\nLine 3");
    EXPECT_EQ(reader.getContextAfter(3, 1, 2), "ine 3\nLine 4");
}

TEST_F(DocumentContextReaderTest, testGetContextBeforeWithCopyright)
{
    auto reader = createTestReader("/* Copyright (C) 2024 */\nLine 0\nLine 1\nLine 2\nLine 3");

    EXPECT_EQ(reader.getContextBefore(0, -1, 2), "");
    EXPECT_EQ(reader.getContextBefore(1, -1, 2), "Line 0");
    EXPECT_EQ(reader.getContextBefore(2, -1, 2), "Line 0\nLine 1");
    EXPECT_EQ(reader.getContextBefore(3, -1, 2), "Line 1\nLine 2");
    EXPECT_EQ(reader.getContextBefore(0, 1, 2), "");
    EXPECT_EQ(reader.getContextBefore(1, 1, 2), "L");
    EXPECT_EQ(reader.getContextBefore(2, 1, 2), "Line 0\nL");
    EXPECT_EQ(reader.getContextBefore(3, 1, 2), "Line 1\nL");
}

TEST_F(DocumentContextReaderTest, testGetContextAfterWithCopyright)
{
    auto reader = createTestReader("/* Copyright (C) 2024 */\nLine 0\nLine 1\nLine 2\nLine 3");

    EXPECT_EQ(reader.getContextAfter(0, -1, 2), "/* Copyright (C) 2024 */\nLine 0");
    EXPECT_EQ(reader.getContextAfter(1, -1, 2), "Line 0\nLine 1");
    EXPECT_EQ(reader.getContextAfter(2, -1, 2), "Line 1\nLine 2");
    EXPECT_EQ(reader.getContextAfter(3, -1, 2), "Line 2\nLine 3");
    EXPECT_EQ(reader.getContextAfter(0, 1, 2), "* Copyright (C) 2024 */\nLine 0");
    EXPECT_EQ(reader.getContextAfter(1, 1, 2), "ine 0\nLine 1");
    EXPECT_EQ(reader.getContextAfter(2, 1, 2), "ine 1\nLine 2");
    EXPECT_EQ(reader.getContextAfter(3, 1, 2), "ine 2\nLine 3");
}

TEST_F(DocumentContextReaderTest, testReadWholeFile)
{
    auto reader = createTestReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    EXPECT_EQ(reader.readWholeFileBefore(2, -1), "Line 0\nLine 1\nLine 2");
    EXPECT_EQ(reader.readWholeFileAfter(2, -1), "Line 2\nLine 3\nLine 4");
}

TEST_F(DocumentContextReaderTest, testReadWholeFileWithCopyright)
{
    auto reader = createTestReader("/* Copyright (C) 2024 */\nLine 0\nLine 1\nLine 2\nLine 3");

    EXPECT_EQ(reader.readWholeFileBefore(2, -1), "Line 0\nLine 1");
    EXPECT_EQ(reader.readWholeFileAfter(2, -1), "Line 1\nLine 2\nLine 3");

    EXPECT_EQ(reader.readWholeFileBefore(2, 0), "Line 0\n");
    EXPECT_EQ(reader.readWholeFileAfter(2, 0), "Line 1\nLine 2\nLine 3");
    EXPECT_EQ(reader.readWholeFileBefore(2, 2), "Line 0\nLi");
    EXPECT_EQ(reader.readWholeFileAfter(2, 2), "ne 1\nLine 2\nLine 3");
}

TEST_F(DocumentContextReaderTest, testReadWholeFileWithMultilineCopyright)
{
    auto reader = createTestReader(
        "/*\n * Copyright (C) 2024\n * \n * This file is part of QodeAssist.\n */\n"
        "Line 0\nLine 1");

    EXPECT_EQ(reader.readWholeFileBefore(6, -1), "Line 0\nLine 1");
    EXPECT_EQ(reader.readWholeFileAfter(5, -1), "Line 0\nLine 1");

    EXPECT_EQ(reader.readWholeFileBefore(6, 0), "Line 0\n");
    EXPECT_EQ(reader.readWholeFileAfter(6, 0), "Line 1");
    EXPECT_EQ(reader.readWholeFileBefore(6, 2), "Line 0\nLi");
    EXPECT_EQ(reader.readWholeFileAfter(6, 2), "ne 1");
}

TEST_F(DocumentContextReaderTest, testFindCopyrightSingleLine)
{
    auto reader = createTestReader("/* Copyright (C) 2024 */\nCode line 0\nCode line 1");

    auto info = reader.findCopyright();
    ASSERT_TRUE(info.found);
    EXPECT_EQ(info.startLine, 0);
    EXPECT_EQ(info.endLine, 0);
}

TEST_F(DocumentContextReaderTest, testFindCopyrightMultiLine)
{
    auto reader = createTestReader(
        "/*\n * Copyright (C) 2024\n * \n * This file is part of QodeAssist.\n */\nCode line 0");

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
    auto reader = createTestReader("/* Just a comment */\nCode line 0");

    auto info = reader.findCopyright();
    ASSERT_TRUE(!info.found);
    EXPECT_EQ(info.startLine, -1);
    EXPECT_EQ(info.endLine, -1);
}

TEST_F(DocumentContextReaderTest, testGetContextBetween)
{
    auto reader = createTestReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    EXPECT_EQ(reader.getContextBetween(2, -1, 0, -1), "");
    EXPECT_EQ(reader.getContextBetween(0, -1, 0, -1), "Line 0");
    EXPECT_EQ(reader.getContextBetween(1, -1, 1, -1), "Line 1");
    EXPECT_EQ(reader.getContextBetween(1, 3, 1, 1), "");
    EXPECT_EQ(reader.getContextBetween(1, 3, 1, 3), "");
    EXPECT_EQ(reader.getContextBetween(1, 3, 1, 4), "e");

    EXPECT_EQ(reader.getContextBetween(1, -1, 3, -1), "Line 1\nLine 2\nLine 3");
    EXPECT_EQ(reader.getContextBetween(1, 2, 3, -1), "ne 1\nLine 2\nLine 3");
    EXPECT_EQ(reader.getContextBetween(1, -1, 3, 2), "Line 1\nLine 2\nLi");
    EXPECT_EQ(reader.getContextBetween(1, 2, 3, 2), "ne 1\nLine 2\nLi");
}

TEST_F(DocumentContextReaderTest, testPrepareContext)
{
    auto reader = createTestReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    EXPECT_EQ(
        reader.prepareContext(2, 3, *createSettingsForWholeFile()),
        (ContextData{
            .prefix = "Line 0\nLine 1\nLin",
            .suffix = "e 2\nLine 3\nLine 4",
            .fileContext = "\n Language:  (MIME: text/python) filepath: /path/to/file()\n\n\n "}));

    EXPECT_EQ(
        reader.prepareContext(2, 3, *createSettingsForLines(1, 1)),
        (ContextData{
            .prefix = "Line 1\nLin",
            .suffix = "e 2\nLine 3",
            .fileContext = "\n Language:  (MIME: text/python) filepath: /path/to/file()\n\n\n "}));

    EXPECT_EQ(
        reader.prepareContext(2, 3, *createSettingsForLines(2, 2)),
        (ContextData{
            .prefix = "Line 0\nLine 1\nLin",
            .suffix = "e 2\nLine 3\nLine 4",
            .fileContext = "\n Language:  (MIME: text/python) filepath: /path/to/file()\n\n\n "}));
}

#include "DocumentContextReaderTest.moc"
