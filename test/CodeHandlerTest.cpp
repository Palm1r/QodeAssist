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

#include "CodeHandler.hpp"
#include "TestUtils.hpp"

#include <gtest/gtest.h>
#include <QObject>
#include <QString>

using namespace QodeAssist;

class CodeHandlerTest : public QObject, public testing::Test
{
    // Tests are written in the fixture format with the expectation that CodeHandler will
    // be expanded.
    Q_OBJECT
};

TEST_F(CodeHandlerTest, testProcessTextEmpty)
{
    EXPECT_EQ(CodeHandler::processText("", "/file.py"), "\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithLanguageCodeBlock)
{
    QString input = "This is a comment\n"
                    "```python\nprint('Hello, world!')\n```\n"
                    "Another comment";

    EXPECT_EQ(
        CodeHandler::processText(input, "/file.py"),
        "# This is a comment\n\nprint('Hello, world!')\n# Another comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithPlainCodeBlockNoNewline)
{
    QString input = "This is a comment\n"
                    "```print('Hello, world!')\n```\n"
                    "Another comment";

    EXPECT_EQ(
        CodeHandler::processText(input, "/file.py"),
        "# This is a comment\n\nprint('Hello, world!')\n# Another comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithPlainCodeBlockWithNewline)
{
    QString input = "This is a comment\n"
                    "```\nprint('Hello, world!')\n```\n"
                    "Another comment";

    EXPECT_EQ(
        CodeHandler::processText(input, "/file.py"),
        "# This is a comment\n\n\nprint('Hello, world!')\n# Another comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextNoCommentsWithLanguageCodeBlock)
{
    QString input = "```python\nprint('Hello, world!')\n```";

    EXPECT_EQ(CodeHandler::processText(input, "/file.py"), "print('Hello, world!')\n");
}

TEST_F(CodeHandlerTest, testProcessTextNoCommentsWithPlainCodeBlockNoNewline)
{
    QString input = "```print('Hello, world!')\n```";

    EXPECT_EQ(CodeHandler::processText(input, "/file.py"), "print('Hello, world!')\n");
}

TEST_F(CodeHandlerTest, testProcessTextNoCommentsWithPlainCodeBlockWithNewline)
{
    QString input = "```\nprint('Hello, world!')\n```";

    EXPECT_EQ(CodeHandler::processText(input, "/file.py"), "\nprint('Hello, world!')\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithMultipleCodeBlocksDifferentLanguages)
{
    QString input = "First comment\n```python\nprint('Block 1')\n"
                    "```\nMiddle comment\n"
                    "```cpp\ncout << \"Block 2\";\n```\n"
                    "Last comment";

    EXPECT_EQ(
        CodeHandler::processText(input, "/file.py"),
        "# First comment\n\n"
        "print('Block 1')\n"
        "// Middle comment\n\n"
        "cout << \"Block 2\";\n"
        "// Last comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithMultipleCodeBlocksSameLanguage)
{
    QString input = "First comment\n```python\nprint('Block 1')\n"
                    "```\nMiddle comment\n"
                    "```python\nprint('Block 2')\n```\n"
                    "Last comment";

    EXPECT_EQ(
        CodeHandler::processText(input, "/file.py"),
        "# First comment\n\n"
        "print('Block 1')\n"
        "# Middle comment\n\n"
        "print('Block 2')\n"
        "# Last comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithMultiplePlainCodeBlocksWithNewline)
{
    QString input = "First comment\n```\nprint('Block 1')\n"
                    "```\nMiddle comment\n"
                    "```\ncout << \"Block 2\";\n```\n"
                    "Last comment";

    EXPECT_EQ(
        CodeHandler::processText(input, "/file.py"),
        "# First comment\n\n\n"
        "print('Block 1')\n"
        "# Middle comment\n\n\n"
        "cout << \"Block 2\";\n"
        "# Last comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithMultiplePlainCodeBlocksWithoutNewline)
{
    QString input = "First comment\n```print('Block 1')\n"
                    "```\nMiddle comment\n"
                    "```cout << \"Block 2\";\n```\n"
                    "Last comment";

    EXPECT_EQ(
        CodeHandler::processText(input, "/file.py"),
        "# First comment\n\n"
        "print('Block 1')\n"
        "# Middle comment\n\n"
        "cout << \"Block 2\";\n"
        "# Last comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithEmptyLines)
{
    QString input = "Comment with empty line\n\n```python\nprint('Hello')\n```\n\nAnother comment";

    EXPECT_EQ(
        CodeHandler::processText(input, "/file.py"),
        "# Comment with empty line\n\n\n"
        "print('Hello')\n\n"
        "# Another comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextPlainCodeBlockWithNewlineWithEmptyLines)
{
    QString input = "Comment with empty line\n\n```\nprint('Hello')\n```\n\nAnother comment";

    EXPECT_EQ(
        CodeHandler::processText(input, "/file.py"),
        "# Comment with empty line\n\n\n\n"
        "print('Hello')\n\n"
        "# Another comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithoutCodeBlock)
{
    QString input = "This is just a comment\nwith multiple lines";

    EXPECT_EQ(
        CodeHandler::processText(input, "/file.py"),
        "# This is just a comment\n# with multiple lines\n\n");
}

TEST_F(CodeHandlerTest, testDetectLanguageFromLine)
{
    EXPECT_EQ(CodeHandler::detectLanguageFromLine("```python"), "python");
    EXPECT_EQ(CodeHandler::detectLanguageFromLine("```javascript"), "js");
    EXPECT_EQ(CodeHandler::detectLanguageFromLine("```cpp"), "c-like");
    EXPECT_EQ(CodeHandler::detectLanguageFromLine("``` ruby "), "ruby");
    EXPECT_EQ(CodeHandler::detectLanguageFromLine("```"), "");
    EXPECT_EQ(CodeHandler::detectLanguageFromLine("``` "), "");
}

TEST_F(CodeHandlerTest, testDetectLanguageFromExtension)
{
    EXPECT_EQ(CodeHandler::detectLanguageFromExtension("py"), "python");
    EXPECT_EQ(CodeHandler::detectLanguageFromExtension("js"), "js");
    EXPECT_EQ(CodeHandler::detectLanguageFromExtension("cpp"), "c-like");
    EXPECT_EQ(CodeHandler::detectLanguageFromExtension("hpp"), "c-like");
    EXPECT_EQ(CodeHandler::detectLanguageFromExtension("rb"), "ruby");
    EXPECT_EQ(CodeHandler::detectLanguageFromExtension("sh"), "shell");
    EXPECT_EQ(CodeHandler::detectLanguageFromExtension("unknown"), "");
    EXPECT_EQ(CodeHandler::detectLanguageFromExtension(""), "");
}

TEST_F(CodeHandlerTest, testCommentPrefixForDifferentLanguages)
{
    struct TestCase
    {
        std::string language;
        QString input;
        QString expected;
    };

    std::vector<TestCase> testCases
        = {{"python", "Comment\n```python\ncode\n```", "# Comment\n\ncode\n"},
           {"cpp", "Comment\n```cpp\ncode\n```", "// Comment\n\ncode\n"},
           {"ruby", "Comment\n```ruby\ncode\n```", "# Comment\n\ncode\n"},
           {"lua", "Comment\n```lua\ncode\n```", "-- Comment\n\ncode\n"}};

    for (const auto &testCase : testCases) {
        EXPECT_EQ(CodeHandler::processText(testCase.input, ""), testCase.expected)
            << "Failed for language: " << testCase.language;
    }
}

#include "CodeHandlerTest.moc"
