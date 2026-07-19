// Copyright (C) 2026 Petr Mironychev
// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QSharedPointer>

#include "context/DocumentContextReader.hpp"
#include "settings/CodeCompletionSettings.hpp"

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

namespace QodeAssist {

class QodeAssistTest final : public QObject
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

    void testCacheControlBreakpointWithoutExtendedTtl();
    void testCacheControlBreakpointWithExtendedTtl();
    void testCacheControlSystemAsStringWrappedIntoArray();
    void testCacheControlEmptySystemStringIsNotWrapped();
    void testCacheControlSystemAsArrayMarksLastBlock();
    void testCacheControlToolsLastEntryGetsCacheControl();
    void testCacheControlSingleMessageHistorySkipped();
    void testCacheControlHistoryBreakpointOnSecondToLastMessage();
    void testCacheControlHistoryArrayContentMarksLastBlock();
    void testCacheControlNoSystemNoToolsNoMessagesIsNoop();
    void testCacheControlEmptyToolsArrayIsNoop();

    void testGetLineText();
    void testGetContext();
    void testGetContextWithCopyright();
    void testReadWholeFile();
    void testReadWholeFileWithCopyright();
    void testReadWholeFileWithMultilineCopyright();
    void testFindCopyrightSingleLine();
    void testFindCopyrightMultiLine();
    void testFindCopyrightMultipleBlocks();
    void testFindCopyrightNoCopyright();
    void testGetContextBetween();
    void testPrepareContext();

    void testHistorySnapshotUsesCurrentVersion();
    void testHistorySnapshotRoundTrip();
    void testUnsupportedChatVersionIsRejected();
    void testChatFileWithoutMessagesArrayIsRejected();
    void testLegacyChatFileConvertsToHistory();
    void testLegacyToolRowWithoutMetadataKeepsItsText();
    void testChatRowsRoundTripThroughHistory();
    void testCompressedChatShapeReloadsAsOneAssistantRow();
    void testHistoryAppliesToChatModel();
    void testAdjacentThinkingBlocksSurviveReload();

    void testSessionAppendsUserTurnBeforeSending();
    void testSessionStreamsTextThinkingAndTools();
    void testSessionTrimsStreamedText();
    void testSessionAbandoningTheTurnCancelsTheBackend();
    void testSessionProjectionMatchesHistory();
    void testSessionAccumulatesStreamedThinking();
    void testSessionKeepsThinkingAfterToolContinuation();
    void testSessionDropsPreToolTextWhenAsked();
    void testSessionToolResultAppendsFileEditRow();
    void testSessionIgnoresEchoedFileEditMarker();
    void testSessionTruncationKeepsBlockOrder();
    void testHistorySnapshotReportsUnreadableBlocks();
    void testSessionUsageLandsOnTheTurn();
    void testSessionIgnoresEventsFromOtherTurns();
    void testSessionReportsFailureWithoutStartedTurn();
    void testSessionIgnoresFailureFromAbandonedTurn();
    void testSessionTruncateRowsRewritesHistory();

    void testTurnContextCollectsPartsFromPorts();
    void testTurnContextSkillCommandScanning();
    void testTurnContextWithoutSkillsPort();
    void testLinkedFilesHeaderSurvivesUnreadablePaths();
    void testSystemPromptRenderingWithProject();
    void testSystemPromptRenderingWithoutProject();

private:
    Context::DocumentContextReader createReader(const QString &text);
    QSharedPointer<Settings::CodeCompletionSettings> createSettingsForWholeFile();
    QSharedPointer<Settings::CodeCompletionSettings> createSettingsForLines(
        int linesBefore, int linesAfter);
    static QJsonObject expectedEphemeral(bool extendedTtl);
};

} // namespace QodeAssist
