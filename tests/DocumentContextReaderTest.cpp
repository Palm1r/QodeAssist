// Copyright (C) 2026 Petr Mironychev
// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "DocumentContextReaderTest.hpp"

#include <QTest>
#include <QTextDocument>

#include "context/DocumentContextReader.hpp"
#include "llmcore/ContextData.hpp"
#include "settings/CodeCompletionSettings.hpp"

namespace QodeAssist {

namespace {

constexpr char kTestMimeType[] = "text/python";
constexpr char kTestFilePath[] = "/path/to/file";
constexpr char kTestFileContext[]
    = "\n Language:  (MIME: text/python) filepath: /path/to/file()\n\n";

} // namespace

Context::DocumentContextReader DocumentContextReaderTest::createReader(const QString &text)
{
    auto *document = new QTextDocument(this);
    document->setPlainText(text);
    return Context::DocumentContextReader(document, kTestMimeType, kTestFilePath);
}

QSharedPointer<Settings::CodeCompletionSettings> DocumentContextReaderTest::createSettingsForWholeFile()
{
    auto settings = QSharedPointer<Settings::CodeCompletionSettings>::create();
    settings->readFullFile.setValue(true, Utils::BaseAspect::BeQuiet);
    return settings;
}

QSharedPointer<Settings::CodeCompletionSettings> DocumentContextReaderTest::createSettingsForLines(
    int linesBefore, int linesAfter)
{
    auto settings = QSharedPointer<Settings::CodeCompletionSettings>::create();
    settings->readFullFile.setValue(false, Utils::BaseAspect::BeQuiet);
    settings->readStringsBeforeCursor.setValue(linesBefore, Utils::BaseAspect::BeQuiet);
    settings->readStringsAfterCursor.setValue(linesAfter, Utils::BaseAspect::BeQuiet);
    return settings;
}

void DocumentContextReaderTest::testGetLineText()
{
    auto reader = createReader("Line 0\nLine 1\nLine 2");

    QCOMPARE(reader.getLineText(0), QString("Line 0"));
    QCOMPARE(reader.getLineText(1), QString("Line 1"));
    QCOMPARE(reader.getLineText(2), QString("Line 2"));
    QCOMPARE(reader.getLineText(0, 4), QString("Line"));
}

void DocumentContextReaderTest::testGetContext()
{
    auto reader = createReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    QCOMPARE(reader.getContextBefore(0, -1, 2), QString("Line 0"));
    QCOMPARE(reader.getContextAfter(0, -1, 2), QString("Line 0\nLine 1"));

    QCOMPARE(reader.getContextBefore(1, -1, 2), QString("Line 0\nLine 1"));
    QCOMPARE(reader.getContextAfter(1, -1, 2), QString("Line 1\nLine 2"));

    QCOMPARE(reader.getContextBefore(2, -1, 2), QString("Line 1\nLine 2"));
    QCOMPARE(reader.getContextAfter(2, -1, 2), QString("Line 2\nLine 3"));

    QCOMPARE(reader.getContextBefore(3, -1, 2), QString("Line 2\nLine 3"));
    QCOMPARE(reader.getContextAfter(3, -1, 2), QString("Line 3\nLine 4"));

    QCOMPARE(reader.getContextBefore(0, 1, 2), QString("L"));
    QCOMPARE(reader.getContextAfter(0, 1, 2), QString("ine 0\nLine 1"));

    QCOMPARE(reader.getContextBefore(1, 1, 2), QString("Line 0\nL"));
    QCOMPARE(reader.getContextAfter(1, 1, 2), QString("ine 1\nLine 2"));

    QCOMPARE(reader.getContextBefore(2, 1, 2), QString("Line 1\nL"));
    QCOMPARE(reader.getContextAfter(2, 1, 2), QString("ine 2\nLine 3"));

    QCOMPARE(reader.getContextBefore(3, 1, 2), QString("Line 2\nL"));
    QCOMPARE(reader.getContextAfter(3, 1, 2), QString("ine 3\nLine 4"));
}

void DocumentContextReaderTest::testGetContextWithCopyright()
{
    auto reader = createReader("/* Copyright (C) 2024 */\nLine 0\nLine 1\nLine 2\nLine 3");

    QCOMPARE(reader.getContextBefore(0, -1, 2), QString());
    QCOMPARE(reader.getContextAfter(0, -1, 2), QString("Line 0"));

    QCOMPARE(reader.getContextBefore(1, -1, 2), QString("Line 0"));
    QCOMPARE(reader.getContextAfter(1, -1, 2), QString("Line 0\nLine 1"));

    QCOMPARE(reader.getContextBefore(2, -1, 2), QString("Line 0\nLine 1"));
    QCOMPARE(reader.getContextAfter(2, -1, 2), QString("Line 1\nLine 2"));

    QCOMPARE(reader.getContextBefore(3, -1, 2), QString("Line 1\nLine 2"));
    QCOMPARE(reader.getContextAfter(3, -1, 2), QString("Line 2\nLine 3"));

    QCOMPARE(reader.getContextBefore(0, 1, 2), QString());
    QCOMPARE(reader.getContextAfter(0, 1, 2), QString("Line 0"));

    QCOMPARE(reader.getContextBefore(1, 1, 2), QString("L"));
    QCOMPARE(reader.getContextAfter(1, 1, 2), QString("ine 0\nLine 1"));

    QCOMPARE(reader.getContextBefore(2, 1, 2), QString("Line 0\nL"));
    QCOMPARE(reader.getContextAfter(2, 1, 2), QString("ine 1\nLine 2"));

    QCOMPARE(reader.getContextBefore(3, 1, 2), QString("Line 1\nL"));
    QCOMPARE(reader.getContextAfter(3, 1, 2), QString("ine 2\nLine 3"));
}

void DocumentContextReaderTest::testReadWholeFile()
{
    auto reader = createReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    QCOMPARE(reader.readWholeFileBefore(0, -1), QString("Line 0"));
    QCOMPARE(reader.readWholeFileAfter(0, -1), QString("Line 0\nLine 1\nLine 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(1, -1), QString("Line 0\nLine 1"));
    QCOMPARE(reader.readWholeFileAfter(1, -1), QString("Line 1\nLine 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(2, -1), QString("Line 0\nLine 1\nLine 2"));
    QCOMPARE(reader.readWholeFileAfter(2, -1), QString("Line 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(3, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));
    QCOMPARE(reader.readWholeFileAfter(3, -1), QString("Line 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(4, -1), QString("Line 0\nLine 1\nLine 2\nLine 3\nLine 4"));
    QCOMPARE(reader.readWholeFileAfter(4, -1), QString("Line 4"));

    QCOMPARE(reader.readWholeFileBefore(0, 1), QString("L"));
    QCOMPARE(reader.readWholeFileAfter(0, 1), QString("ine 0\nLine 1\nLine 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(1, 1), QString("Line 0\nL"));
    QCOMPARE(reader.readWholeFileAfter(1, 1), QString("ine 1\nLine 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(2, 1), QString("Line 0\nLine 1\nL"));
    QCOMPARE(reader.readWholeFileAfter(2, 1), QString("ine 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(3, 1), QString("Line 0\nLine 1\nLine 2\nL"));
    QCOMPARE(reader.readWholeFileAfter(3, 1), QString("ine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(4, 1), QString("Line 0\nLine 1\nLine 2\nLine 3\nL"));
    QCOMPARE(reader.readWholeFileAfter(4, 1), QString("ine 4"));
}

void DocumentContextReaderTest::testReadWholeFileWithCopyright()
{
    auto reader = createReader("/* Copyright (C) 2024 */\nLine 0\nLine 1\nLine 2\nLine 3");

    QCOMPARE(reader.readWholeFileBefore(0, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(0, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, -1), QString("Line 0"));
    QCOMPARE(reader.readWholeFileAfter(1, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, -1), QString("Line 0\nLine 1"));
    QCOMPARE(reader.readWholeFileAfter(2, -1), QString("Line 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, -1), QString("Line 0\nLine 1\nLine 2"));
    QCOMPARE(reader.readWholeFileAfter(3, -1), QString("Line 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(0, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(0, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(1, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, 0), QString("Line 0\n"));
    QCOMPARE(reader.readWholeFileAfter(2, 0), QString("Line 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, 0), QString("Line 0\nLine 1\n"));
    QCOMPARE(reader.readWholeFileAfter(3, 0), QString("Line 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(0, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(0, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, 1), QString("L"));
    QCOMPARE(reader.readWholeFileAfter(1, 1), QString("ine 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, 1), QString("Line 0\nL"));
    QCOMPARE(reader.readWholeFileAfter(2, 1), QString("ine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, 1), QString("Line 0\nLine 1\nL"));
    QCOMPARE(reader.readWholeFileAfter(3, 1), QString("ine 2\nLine 3"));
}

void DocumentContextReaderTest::testReadWholeFileWithMultilineCopyright()
{
    auto reader = createReader(
        "/*\n * Copyright (C) 2024\n * \n * This file is part of QodeAssist.\n */\n"
        "Line 0\nLine 1\nLine 2\nLine 3");

    QCOMPARE(reader.readWholeFileBefore(0, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(0, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(1, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(2, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(3, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(4, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(4, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(5, -1), QString("Line 0"));
    QCOMPARE(reader.readWholeFileAfter(5, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(6, -1), QString("Line 0\nLine 1"));
    QCOMPARE(reader.readWholeFileAfter(6, -1), QString("Line 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(7, -1), QString("Line 0\nLine 1\nLine 2"));
    QCOMPARE(reader.readWholeFileAfter(7, -1), QString("Line 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(8, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));
    QCOMPARE(reader.readWholeFileAfter(8, -1), QString("Line 3"));

    QCOMPARE(reader.readWholeFileBefore(0, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(0, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(1, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(2, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(3, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(4, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(4, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(5, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(5, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(6, 0), QString("Line 0\n"));
    QCOMPARE(reader.readWholeFileAfter(6, 0), QString("Line 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(7, 0), QString("Line 0\nLine 1\n"));
    QCOMPARE(reader.readWholeFileAfter(7, 0), QString("Line 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(8, 0), QString("Line 0\nLine 1\nLine 2\n"));
    QCOMPARE(reader.readWholeFileAfter(8, 0), QString("Line 3"));

    QCOMPARE(reader.readWholeFileBefore(0, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(0, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(1, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(2, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(3, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(4, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(4, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(5, 1), QString("L"));
    QCOMPARE(reader.readWholeFileAfter(5, 1), QString("ine 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(6, 1), QString("Line 0\nL"));
    QCOMPARE(reader.readWholeFileAfter(6, 1), QString("ine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(7, 1), QString("Line 0\nLine 1\nL"));
    QCOMPARE(reader.readWholeFileAfter(7, 1), QString("ine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(8, 1), QString("Line 0\nLine 1\nLine 2\nL"));
    QCOMPARE(reader.readWholeFileAfter(8, 1), QString("ine 3"));
}

void DocumentContextReaderTest::testFindCopyrightSingleLine()
{
    auto reader = createReader("/* Copyright (C) 2024 */\nCode line 0\nCode line 1");

    const auto info = reader.findCopyright();
    QVERIFY(info.found);
    QCOMPARE(info.startLine, 0);
    QCOMPARE(info.endLine, 0);
}

void DocumentContextReaderTest::testFindCopyrightMultiLine()
{
    auto reader = createReader(
        "/*\n * Copyright (C) 2024\n * \n * This file is part of QodeAssist.\n */\nCode line 0");

    const auto info = reader.findCopyright();
    QVERIFY(info.found);
    QCOMPARE(info.startLine, 0);
    QCOMPARE(info.endLine, 4);
}

void DocumentContextReaderTest::testFindCopyrightMultipleBlocks()
{
    auto reader = createReader("/* Copyright 2023 */\n\n/* Copyright 2024 */\nCode");

    const auto info = reader.findCopyright();
    QVERIFY(info.found);
    QCOMPARE(info.startLine, 0);
    QCOMPARE(info.endLine, 0);
}

void DocumentContextReaderTest::testFindCopyrightNoCopyright()
{
    auto reader = createReader("/* Just a comment */\nCode line 0");

    const auto info = reader.findCopyright();
    QVERIFY(!info.found);
    QCOMPARE(info.startLine, -1);
    QCOMPARE(info.endLine, -1);
}

void DocumentContextReaderTest::testGetContextBetween()
{
    auto reader = createReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    QCOMPARE(reader.getContextBetween(2, -1, 0, -1), QString());
    QCOMPARE(reader.getContextBetween(0, -1, 0, -1), QString("Line 0"));
    QCOMPARE(reader.getContextBetween(1, -1, 1, -1), QString("Line 1"));
    QCOMPARE(reader.getContextBetween(1, 3, 1, 1), QString());
    QCOMPARE(reader.getContextBetween(1, 3, 1, 3), QString());
    QCOMPARE(reader.getContextBetween(1, 3, 1, 4), QString("e"));

    QCOMPARE(reader.getContextBetween(1, -1, 3, -1), QString("Line 1\nLine 2\nLine 3"));
    QCOMPARE(reader.getContextBetween(1, 2, 3, -1), QString("ne 1\nLine 2\nLine 3"));
    QCOMPARE(reader.getContextBetween(1, -1, 3, 2), QString("Line 1\nLine 2\nLi"));
    QCOMPARE(reader.getContextBetween(1, 2, 3, 2), QString("ne 1\nLine 2\nLi"));
}

void DocumentContextReaderTest::testPrepareContext()
{
    auto reader = createReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    QCOMPARE(
        reader.prepareContext(2, 3, *createSettingsForWholeFile()),
        (LLMCore::ContextData{
            .prefix = "Line 0\nLine 1\nLin",
            .suffix = "e 2\nLine 3\nLine 4",
            .fileContext = kTestFileContext}));

    QCOMPARE(
        reader.prepareContext(2, 3, *createSettingsForLines(1, 1)),
        (LLMCore::ContextData{
            .prefix = "Line 1\nLin", .suffix = "e 2\nLine 3", .fileContext = kTestFileContext}));

    QCOMPARE(
        reader.prepareContext(2, 3, *createSettingsForLines(2, 2)),
        (LLMCore::ContextData{
            .prefix = "Line 0\nLine 1\nLin",
            .suffix = "e 2\nLine 3\nLine 4",
            .fileContext = kTestFileContext}));
}

} // namespace QodeAssist
