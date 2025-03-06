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

TEST_F(CodeHandlerTest, testProcessTextWithCodeBlock)
{
    QString input = "This is a comment\n"
                    "```python\nprint('Hello, world!')\n```\n"
                    "Another comment";

    EXPECT_EQ(
        CodeHandler::processText(input),
        "# This is a comment\n\nprint('Hello, world!')\n# Another comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithMultipleCodeBlocks)
{
    QString input = "First comment\n```python\nprint('Block 1')\n"
                    "```\nMiddle comment\n"
                    "```cpp\ncout << \"Block 2\";\n```\n"
                    "Last comment";

    EXPECT_EQ(
        CodeHandler::processText(input),
        "# First comment\n\n"
        "print('Block 1')\n"
        "// Middle comment\n\n"
        "cout << \"Block 2\";\n"
        "// Last comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithEmptyLines)
{
    QString input = "Comment with empty line\n\n```python\nprint('Hello')\n```\n\nAnother comment";

    EXPECT_EQ(
        CodeHandler::processText(input),
        "# Comment with empty line\n\n\nprint('Hello')\n\n# Another comment\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithoutCodeBlock)
{
    QString input = "This is just a comment\nwith multiple lines";

    EXPECT_EQ(
        CodeHandler::processText(input), "// This is just a comment\n// with multiple lines\n\n");
}

TEST_F(CodeHandlerTest, testProcessTextWithDifferentLanguages)
{
    QString input = "Python code:\n"
                    "```python\nprint('Hello')\n```\n"
                    "JavaScript code:\n"
                    "```javascript\nconsole.log('Hello');\n```";

    EXPECT_EQ(
        CodeHandler::processText(input),
        "# Python code:\n\nprint('Hello')\n"
        "// JavaScript code:\n\nconsole.log('Hello');\n");
}

TEST_F(CodeHandlerTest, testDetectLanguage)
{
    EXPECT_EQ(CodeHandler::detectLanguage("```python"), "python");
    EXPECT_EQ(CodeHandler::detectLanguage("```javascript"), "javascript");
    EXPECT_EQ(CodeHandler::detectLanguage("```cpp"), "cpp");
    EXPECT_EQ(CodeHandler::detectLanguage("``` ruby "), "ruby");
    EXPECT_EQ(CodeHandler::detectLanguage("```"), "");
    EXPECT_EQ(CodeHandler::detectLanguage("``` "), "");
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
        EXPECT_EQ(CodeHandler::processText(testCase.input), testCase.expected)
            << "Failed for language: " << testCase.language;
    }
}

TEST_F(CodeHandlerTest, testEmptyInput)
{
    EXPECT_EQ(CodeHandler::processText(""), "\n\n");
}

TEST_F(CodeHandlerTest, testCodeBlockWithoutLanguage)
{
    QString input = "Comment\n```\ncode\n```";

    EXPECT_EQ(CodeHandler::processText(input), "// Comment\n\ncode\n");
}

#include "CodeHandlerTest.moc"
