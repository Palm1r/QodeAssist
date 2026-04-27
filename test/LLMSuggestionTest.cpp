// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

// Degenerate / no-op cases
TEST_F(LLMSuggestionTest, emptyRight)
{
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("foo", ""), 0);
}

TEST_F(LLMSuggestionTest, noOverlap)
{
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("foo", "bar"), 0);
}

TEST_F(LLMSuggestionTest, singleCharNoOverlap)
{
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("a", "b"), 0);
}

// LCP: model echoed the existing right side as its own prefix
TEST_F(LLMSuggestionTest, lcpExtendsRight)
{
    // cursor: int |myVariable ; suggestion echoes "myVar" then adds nothing.
    // LCP=5, remainder "iable" not a closing tail -> replace 5.
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("myVar", "myVariable"), 5);
}

TEST_F(LLMSuggestionTest, lcpPartialEngineCall)
{
    // cursor: |engine.rootContext() ; suggestion: engine.load()
    // LCP="engine." (7); remainder "rootContext()" is not closing-only -> keep LCP
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("engine.load()", "engine.rootContext()"), 7);
}

// LCP + closing-only tail: extend to full right
TEST_F(LLMSuggestionTest, lcpThenClosingTailExtendsFull)
{
    // cursor: QStringList |colors{}; ; suggestion: colors << "red"
    // LCP=6 ("colors"); remainder "{};" is punctuation -> replace all (9)
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("colors << \"red\"", "colors{};"), 9);
}

// LCP=0, right is closing-only, suggestion ends with a matching closer -> replace full right
TEST_F(LLMSuggestionTest, closingTailReplaceSemicolon)
{
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("x;", ";"), 1);
}

TEST_F(LLMSuggestionTest, closingTailReplaceParen)
{
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("arg1, arg2)", ")"), 1);
}

TEST_F(LLMSuggestionTest, closingTailReplaceBrackets)
{
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("[0]", "];"), 2);
}

TEST_F(LLMSuggestionTest, closingTailReplaceBracesAndSemi)
{
    // cursor: colors|{}; ; suggestion: = {"red"}
    // LCP=0; right "{};" is closing-only; suggestion ends on "}" which is in right -> replace 3
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("= {\"red\"}", "{};"), 3);
}

TEST_F(LLMSuggestionTest, closingTailWithLeadingSpace)
{
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength(" = {\"red\"}", "{};"), 3);
}

TEST_F(LLMSuggestionTest, closingTailEqualsSemi)
{
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("= 5;", ";"), 1);
}

// LCP=0, right is closing-only but suggestion does not end on any of its chars -> leave right alone
TEST_F(LLMSuggestionTest, closingTailNoMatchingClose)
{
    // right "};" closes; suggestion ends on '"' (not in close set) -> 0
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("\"red\", \"green\"", "};"), 0);
}

// Right side has real code (not closing-only) and no LCP -> leave it alone
TEST_F(LLMSuggestionTest, realCodeRightNoLcp)
{
    // cursor: int |myVar = 5; ; suggestion: myVar
    // LCP=0, right " = 5;" is not closing-only (has '5') -> 0
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("myVar", " = 5;"), 0);
}

// Trailing whitespace is not treated as a closer, suggestion has no closer -> leave alone
TEST_F(LLMSuggestionTest, trailingWhitespaceOnlyLeftAlone)
{
    EXPECT_EQ(LLMSuggestion::calculateReplaceLength("code", "   "), 0);
}

#include "LLMSuggestionTest.moc"
