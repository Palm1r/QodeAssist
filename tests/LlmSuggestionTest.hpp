// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class LlmSuggestionTest final : public QObject
{
    Q_OBJECT

private slots:
    void testReplaceLengthEmptyRight();
    void testReplaceLengthNoOverlap();
    void testReplaceLengthSingleCharNoOverlap();
    void testReplaceLengthLcpExtendsRight();
    void testReplaceLengthLcpPartialEngineCall();
    void testReplaceLengthLcpThenClosingTailExtendsFull();
    void testReplaceLengthClosingTailReplaceSemicolon();
    void testReplaceLengthClosingTailReplaceParen();
    void testReplaceLengthClosingTailReplaceBrackets();
    void testReplaceLengthClosingTailReplaceBracesAndSemi();
    void testReplaceLengthClosingTailWithLeadingSpace();
    void testReplaceLengthClosingTailEqualsSemi();
    void testReplaceLengthClosingTailNoMatchingClose();
    void testReplaceLengthRealCodeRightNoLcp();
    void testReplaceLengthTrailingWhitespaceOnlyLeftAlone();
};

} // namespace QodeAssist
