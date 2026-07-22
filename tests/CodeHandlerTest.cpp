// Copyright (C) 2026 Petr Mironychev
// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "CodeHandlerTest.hpp"

#include <QTest>

#include "completion/CodeHandler.hpp"

namespace QodeAssist {

void CodeHandlerTest::testProcessTextEmpty()
{
    QCOMPARE(CodeHandler::processText("", "/file.py"), QString("\n\n"));
}

void CodeHandlerTest::testProcessTextCommentsAroundCodeBlock()
{
    const QString input = "This is a comment\n"
                          "```python\nprint('Hello, world!')\n```\n"
                          "Another comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString("# This is a comment\n\nprint('Hello, world!')\n# Another comment\n\n"));
}

void CodeHandlerTest::testProcessTextWithPlainCodeBlockNoNewline()
{
    const QString input = "This is a comment\n"
                          "```print('Hello, world!')\n```\n"
                          "Another comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString("# This is a comment\n\nprint('Hello, world!')\n# Another comment\n\n"));
}

void CodeHandlerTest::testProcessTextWithPlainCodeBlockWithNewline()
{
    const QString input = "This is a comment\n"
                          "```\nprint('Hello, world!')\n```\n"
                          "Another comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString("# This is a comment\n\n\nprint('Hello, world!')\n# Another comment\n\n"));
}

void CodeHandlerTest::testProcessTextNoCommentsWithLanguageCodeBlock()
{
    const QString input = "```python\nprint('Hello, world!')\n```";

    QCOMPARE(CodeHandler::processText(input, "/file.py"), QString("print('Hello, world!')\n"));
}

void CodeHandlerTest::testProcessTextNoCommentsWithPlainCodeBlockNoNewline()
{
    const QString input = "```print('Hello, world!')\n```";

    QCOMPARE(CodeHandler::processText(input, "/file.py"), QString("print('Hello, world!')\n"));
}

void CodeHandlerTest::testProcessTextNoCommentsWithPlainCodeBlockWithNewline()
{
    const QString input = "```\nprint('Hello, world!')\n```";

    QCOMPARE(CodeHandler::processText(input, "/file.py"), QString("\nprint('Hello, world!')\n"));
}

void CodeHandlerTest::testProcessTextWithMultipleCodeBlocksDifferentLanguages()
{
    const QString input = "First comment\n```python\nprint('Block 1')\n"
                          "```\nMiddle comment\n"
                          "```cpp\ncout << \"Block 2\";\n```\n"
                          "Last comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# First comment\n\n"
            "print('Block 1')\n"
            "// Middle comment\n\n"
            "cout << \"Block 2\";\n"
            "// Last comment\n\n"));
}

void CodeHandlerTest::testProcessTextWithMultipleCodeBlocksSameLanguage()
{
    const QString input = "First comment\n```python\nprint('Block 1')\n"
                          "```\nMiddle comment\n"
                          "```python\nprint('Block 2')\n```\n"
                          "Last comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# First comment\n\n"
            "print('Block 1')\n"
            "# Middle comment\n\n"
            "print('Block 2')\n"
            "# Last comment\n\n"));
}

void CodeHandlerTest::testProcessTextWithMultiplePlainCodeBlocksWithNewline()
{
    const QString input = "First comment\n```\nprint('Block 1')\n"
                          "```\nMiddle comment\n"
                          "```\ncout << \"Block 2\";\n```\n"
                          "Last comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# First comment\n\n\n"
            "print('Block 1')\n"
            "# Middle comment\n\n\n"
            "cout << \"Block 2\";\n"
            "# Last comment\n\n"));
}

void CodeHandlerTest::testProcessTextWithMultiplePlainCodeBlocksWithoutNewline()
{
    const QString input = "First comment\n```print('Block 1')\n"
                          "```\nMiddle comment\n"
                          "```cout << \"Block 2\";\n```\n"
                          "Last comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# First comment\n\n"
            "print('Block 1')\n"
            "# Middle comment\n\n"
            "cout << \"Block 2\";\n"
            "# Last comment\n\n"));
}

void CodeHandlerTest::testProcessTextWithEmptyLines()
{
    const QString input
        = "Comment with empty line\n\n```python\nprint('Hello')\n```\n\nAnother comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# Comment with empty line\n\n\n"
            "print('Hello')\n\n"
            "# Another comment\n\n"));
}

void CodeHandlerTest::testProcessTextPlainCodeBlockWithNewlineWithEmptyLines()
{
    const QString input = "Comment with empty line\n\n```\nprint('Hello')\n```\n\nAnother comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# Comment with empty line\n\n\n\n"
            "print('Hello')\n\n"
            "# Another comment\n\n"));
}

void CodeHandlerTest::testProcessTextWithoutCodeBlock()
{
    const QString input = "This is just a comment\nwith multiple lines";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString("# This is just a comment\n# with multiple lines\n\n"));
}

void CodeHandlerTest::testDetectLanguageFromLine()
{
    QCOMPARE(CodeHandler::detectLanguageFromLine("```python"), QString("python"));
    QCOMPARE(CodeHandler::detectLanguageFromLine("```javascript"), QString("js"));
    QCOMPARE(CodeHandler::detectLanguageFromLine("```cpp"), QString("c-like"));
    QCOMPARE(CodeHandler::detectLanguageFromLine("``` ruby "), QString("ruby"));
    QCOMPARE(CodeHandler::detectLanguageFromLine("```"), QString());
    QCOMPARE(CodeHandler::detectLanguageFromLine("``` "), QString());
}

void CodeHandlerTest::testDetectLanguageFromExtension()
{
    QCOMPARE(CodeHandler::detectLanguageFromExtension("py"), QString("python"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("js"), QString("js"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("cpp"), QString("c-like"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("hpp"), QString("c-like"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("rb"), QString("ruby"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("sh"), QString("shell"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("unknown"), QString());
    QCOMPARE(CodeHandler::detectLanguageFromExtension(""), QString());
}

void CodeHandlerTest::testCommentPrefixForDifferentLanguages()
{
    struct Case
    {
        QString input;
        QString expected;
    };

    const QList<Case> cases{
        {"Comment\n```python\ncode\n```", "# Comment\n\ncode\n"},
        {"Comment\n```cpp\ncode\n```", "// Comment\n\ncode\n"},
        {"Comment\n```ruby\ncode\n```", "# Comment\n\ncode\n"},
        {"Comment\n```lua\ncode\n```", "-- Comment\n\ncode\n"}};

    for (const auto &testCase : cases)
        QCOMPARE(CodeHandler::processText(testCase.input, ""), testCase.expected);
}

void CodeHandlerTest::testHasCodeBlocks()
{
    QVERIFY(CodeHandler::hasCodeBlocks("text\n```python\ncode\n```"));
    QVERIFY(!CodeHandler::hasCodeBlocks("just a plain comment\nwith two lines"));
}

} // namespace QodeAssist
