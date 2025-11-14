/*
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include "LLMSuggestion.hpp"
#include "TestUtils.hpp"

#include <gtest/gtest.h>
#include <QObject>
#include <QString>

using namespace QodeAssist;

class LLMSuggestionTest : public QObject, public testing::Test
{
    Q_OBJECT
};

// Basic tests
TEST_F(LLMSuggestionTest, testCalculateReplaceLengthEmptyRight)
{
    int result = LLMSuggestion::calculateReplaceLength("foo", "", "foo");
    EXPECT_EQ(result, 0); // No rightText to replace
}

TEST_F(LLMSuggestionTest, testCalculateReplaceLengthNoOverlap)
{
    // No structural or token overlap
    int result = LLMSuggestion::calculateReplaceLength("foo", "bar", "foobar");
    EXPECT_EQ(result, 0); // Just insert, don't replace
}

// Structural overlap tests
TEST_F(LLMSuggestionTest, testCalculateReplaceLengthStructuralBraces)
{
    // suggestion contains {}, rightText contains {}
    int result = LLMSuggestion::calculateReplaceLength("= {\"red\"}", "{};", "colors{};");
    EXPECT_EQ(result, 3); // Replace all rightText
}

TEST_F(LLMSuggestionTest, testCalculateReplaceLengthStructuralSemicolon)
{
    // suggestion contains ;, rightText contains ;
    int result = LLMSuggestion::calculateReplaceLength("x;", ";", "int x;");
    EXPECT_EQ(result, 1); // Replace the ;
}

TEST_F(LLMSuggestionTest, testCalculateReplaceLengthStructuralParens)
{
    // suggestion contains (), rightText contains )
    int result = LLMSuggestion::calculateReplaceLength("arg1, arg2)", ")", "foo(arg1, arg2)");
    EXPECT_EQ(result, 1); // Replace the )
}

TEST_F(LLMSuggestionTest, testCalculateReplaceLengthStructuralBrackets)
{
    // suggestion contains [], rightText contains ]
    int result = LLMSuggestion::calculateReplaceLength("[0]", "];", "arr[0];");
    EXPECT_EQ(result, 2); // Replace ];
}

// Token overlap tests
TEST_F(LLMSuggestionTest, testCalculateReplaceLengthCommonToken)
{
    // suggestion contains "colors", entireLine contains "colors"
    int result = LLMSuggestion::calculateReplaceLength("colors << \"red\"", "colors{};", "QStringList colors{};");
    EXPECT_EQ(result, 9); // Replace all rightText due to common token
}

TEST_F(LLMSuggestionTest, testCalculateReplaceLengthMultipleCommonTokens)
{
    // Multiple tokens in common
    int result = LLMSuggestion::calculateReplaceLength("engine.load()", "engine.rootContext()", "QmlEngine engine.rootContext()");
    EXPECT_EQ(result, 20); // Replace all rightText
}

// Real-world scenarios
TEST_F(LLMSuggestionTest, testCursorInBraces)
{
    // Cursor in braces: QStringList colors{<cursor>};
    // LLM sends: "\"red\", \"green\"", rightText: "};"
    // No common tokens ("red" and "green" are strings, not identifiers in entireLine)
    // No structural overlap (suggestion doesn't contain } or ;)
    int result = LLMSuggestion::calculateReplaceLength("\"red\", \"green\"", "};", "QStringList colors{};");
    EXPECT_EQ(result, 0); // No overlap, just insert
}

TEST_F(LLMSuggestionTest, testCursorBeforeBraces)
{
    // Cursor before braces: QStringList colors<cursor>{};
    // LLM sends: " = {\"red\"}", rightText: "{};"
    int result = LLMSuggestion::calculateReplaceLength(" = {\"red\"}", "{};", "QStringList colors{};");
    EXPECT_EQ(result, 3); // Structural overlap - replace all
}

TEST_F(LLMSuggestionTest, testCursorAfterType)
{
    // Cursor after type: QStringList <cursor>colors{};
    // LLM sends: "colors << \"red\"", rightText: "colors{};"
    int result = LLMSuggestion::calculateReplaceLength("colors << \"red\"", "colors{};", "QStringList colors{};");
    EXPECT_EQ(result, 9); // Common token - replace all
}

TEST_F(LLMSuggestionTest, testCursorInMiddleNoConflict)
{
    // Cursor in middle: int <cursor>myVar = 5;
    // LLM sends: "myVar", rightText: " = 5;", entireLine: "int myVar = 5;"
    // "myVar" is a common token -> replace rightText
    int result = LLMSuggestion::calculateReplaceLength("myVar", " = 5;", "int myVar = 5;");
    EXPECT_EQ(result, 5); // Common token found, replace all rightText
}

TEST_F(LLMSuggestionTest, testCursorWithEqualsSign)
{
    // LLM sends code with = and ;
    int result = LLMSuggestion::calculateReplaceLength("= 5;", ";", "int x;");
    EXPECT_EQ(result, 1); // Structural overlap on ;
}

// Edge cases
TEST_F(LLMSuggestionTest, testNoStructuralButHasToken)
{
    // Token overlap but no structural
    int result = LLMSuggestion::calculateReplaceLength("myVar", "myVariable", "int myVariable");
    EXPECT_EQ(result, 0); // No structural overlap, tokens too different (length > 1 check)
}

TEST_F(LLMSuggestionTest, testOnlyWhitespace)
{
    // rightText is just whitespace, but "code" is common token
    int result = LLMSuggestion::calculateReplaceLength("code", "   ", "code   ");
    EXPECT_EQ(result, 3); // Common token "code", replace rightText
}

TEST_F(LLMSuggestionTest, testSingleCharTokenIgnored)
{
    // Tokens must be > 1 character
    int result = LLMSuggestion::calculateReplaceLength("a", "b", "ab");
    EXPECT_EQ(result, 0); // Single char tokens ignored
}

#include "LLMSuggestionTest.moc"
