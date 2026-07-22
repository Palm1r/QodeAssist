// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "LlmSuggestionTest.hpp"

#include <QTest>

#include "completion/LLMSuggestion.hpp"

namespace QodeAssist {

void LlmSuggestionTest::testReplaceLengthEmptyRight()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("foo", ""), 0);
}

void LlmSuggestionTest::testReplaceLengthNoOverlap()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("foo", "bar"), 0);
}

void LlmSuggestionTest::testReplaceLengthSingleCharNoOverlap()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("a", "b"), 0);
}

void LlmSuggestionTest::testReplaceLengthLcpExtendsRight()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("myVar", "myVariable"), 5);
}

void LlmSuggestionTest::testReplaceLengthLcpPartialEngineCall()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("engine.load()", "engine.rootContext()"), 7);
}

void LlmSuggestionTest::testReplaceLengthLcpThenClosingTailExtendsFull()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("colors << \"red\"", "colors{};"), 9);
}

void LlmSuggestionTest::testReplaceLengthClosingTailReplaceSemicolon()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("x;", ";"), 1);
}

void LlmSuggestionTest::testReplaceLengthClosingTailReplaceParen()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("arg1, arg2)", ")"), 1);
}

void LlmSuggestionTest::testReplaceLengthClosingTailReplaceBrackets()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("[0]", "];"), 2);
}

void LlmSuggestionTest::testReplaceLengthClosingTailReplaceBracesAndSemi()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("= {\"red\"}", "{};"), 3);
}

void LlmSuggestionTest::testReplaceLengthClosingTailWithLeadingSpace()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength(" = {\"red\"}", "{};"), 3);
}

void LlmSuggestionTest::testReplaceLengthClosingTailEqualsSemi()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("= 5;", ";"), 1);
}

void LlmSuggestionTest::testReplaceLengthClosingTailNoMatchingClose()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("\"red\", \"green\"", "};"), 0);
}

void LlmSuggestionTest::testReplaceLengthRealCodeRightNoLcp()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("myVar", " = 5;"), 0);
}

void LlmSuggestionTest::testReplaceLengthTrailingWhitespaceOnlyLeftAlone()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("code", "   "), 0);
}

} // namespace QodeAssist
