// Copyright (C) 2026 Petr Mironychev
// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class CodeHandlerTest final : public QObject
{
    Q_OBJECT

private slots:
    void testProcessTextEmpty();
    void testProcessTextCommentsAroundCodeBlock();
    void testProcessTextWithPlainCodeBlockNoNewline();
    void testProcessTextWithPlainCodeBlockWithNewline();
    void testProcessTextNoCommentsWithLanguageCodeBlock();
    void testProcessTextNoCommentsWithPlainCodeBlockNoNewline();
    void testProcessTextNoCommentsWithPlainCodeBlockWithNewline();
    void testProcessTextWithMultipleCodeBlocksDifferentLanguages();
    void testProcessTextWithMultipleCodeBlocksSameLanguage();
    void testProcessTextWithMultiplePlainCodeBlocksWithNewline();
    void testProcessTextWithMultiplePlainCodeBlocksWithoutNewline();
    void testProcessTextWithEmptyLines();
    void testProcessTextPlainCodeBlockWithNewlineWithEmptyLines();
    void testProcessTextWithoutCodeBlock();
    void testDetectLanguageFromLine();
    void testDetectLanguageFromExtension();
    void testCommentPrefixForDifferentLanguages();
    void testHasCodeBlocks();
};

} // namespace QodeAssist
