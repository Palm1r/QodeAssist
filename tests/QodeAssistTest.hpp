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
    void testSwitchingBackendsCancelsTheActiveTurn();
    void testSessionForwardsTitleFromTheEventStream();
    void testSessionProjectionMatchesHistory();
    void testSessionAccumulatesStreamedThinking();
    void testSessionKeepsThinkingAfterToolContinuation();
    void testSessionDropsPreToolTextWhenAsked();
    void testSessionToolResultAppendsFileEditRow();
    void testSessionRepeatedToolResultKeepsOneFileEdit();
    void testRunningToolStatusInterruptsOnTurnEnd();
    void testRunningToolStatusInterruptsOnLoad();
    void testAgentDiffBecomesAppliedFileEdit();
    void testAgentDiffSkipsUnrevertableRegistrations();
    void testAgentFileEditRevertRoundTrip();
    void testSessionIgnoresEchoedFileEditMarker();
    void testSessionTruncationKeepsBlockOrder();
    void testHistorySnapshotReportsUnreadableBlocks();
    void testSessionUsageLandsOnTheTurn();
    void testSessionIgnoresEventsFromOtherTurns();
    void testSessionReportsFailureWithoutStartedTurn();
    void testSessionIgnoresFailureFromAbandonedTurn();
    void testSessionTruncateRowsRewritesHistory();

    void testSessionUpsertsAgentToolCalls();
    void testLlmToolLifecycleFlowsThroughOneEvent();
    void testSessionKeepsOnePlanPerTurn();
    void testAgentActivityKeepsStreamedTextIntact();
    void testAgentToolUpdateTouchesOnlyItsOwnRow();
    void testUsageSkipsRowsThatCannotShowIt();
    void testAgentActivityBlocksSurviveReload();

    void testEveryRowKindProjectsInOrder();
    void testPromptAudienceKeepsToolsAndThinking();
    void testCompressionAudienceDropsToolsAndThinking();
    void testTokenCountAudienceCountsToolsAndThinking();

    void testSessionRendersPermissionRequestWithItsOptions();
    void testSessionForwardsPermissionAnswerToTheBackend();
    void testSessionAllowAlwaysSuppressesRepeatPromptsOfTheSameKind();
    void testSessionAllowAlwaysDoesNotCoverOtherToolKinds();
    void testSessionNeverAutoAnswersUnclassifiedTools();
    void testSessionIgnoresAnswersToUnknownPermissionRequests();
    void testSessionMarksUnansweredPermissionsCancelled();
    void testSessionRejectAlwaysSuppressesRepeatPromptsOfTheSameKind();
    void testSessionAutoAnswerFallsBackToTheAlwaysOption();
    void testSessionDoesNotAutoAnswerWithoutAMatchingOption();
    void testSessionAllowAlwaysDoesNotSurviveAnotherConversation();
    void testSessionAnswersTheLiveRequestWhenAnOldIdIsReused();
    void testSessionCancelsPermissionsTheBackendNeverResolved();
    void testSessionCancelsPermissionsTheBackendRefused();
    void testPermissionBlockReopensAsUnanswerable();

    void testTurnContextCollectsPartsFromPorts();
    void testTurnContextSkipsWhatTheBackendDoesNotNeed();
    void testFencedFileBlockOutgrowsBackticksInContent();
    void testTurnContextSkillCommandScanning();
    void testTurnContextWithoutSkillsPort();
    void testSystemPromptRenderingWithProject();
    void testSystemPromptRenderingWithoutProject();

    void testAgentRegistryParsesEveryDistributionKind();
    void testAgentRegistryReportsUnusableEntries();
    void testAgentRegistryParsesSingleAgentUserFile();
    void testAgentCatalogAppliesMergePriority();
    void testAgentCatalogUserFileMakesBinaryAgentLaunchable();
    void testAgentLaunchConfigUsesRunnerConventions();
    void testAgentSearchPathsSplitting();
    void testAgentExtraSearchPathsReachTheChildEnvironment();
    void testAgentForwardedVariablesReachTheChildEnvironment();
    void testBundledAgentSnapshotParses();

    void testAcpBackendStreamsTextAndThinking();
    void testAcpBackendStartsSessionInWorkingDirectory();
    void testAcpBackendAuthenticatesWhenTheAgentAsksForIt();
    void testAcpBackendReportsPromptFailure();
    void testAcpBackendCancelInterruptsTheTurn();
    void testAcpBackendSendsAttachmentsAsEmbeddedResources();
    void testAcpBackendInlinesAttachmentsWithoutEmbeddedContextSupport();
    void testAcpBackendReportsAttachmentsItCannotLoad();
    void testAcpBackendSendsImagesWhenTheAgentAcceptsThem();
    void testAcpBackendNamesImagesTheAgentCannotAccept();
    void testAcpBackendFencesAttachmentsThatContainFences();
    void testAcpBackendTruncatesOversizedAttachments();
    void testAcpBackendRejectsAnEmptyTurn();
    void testAcpBackendSendsAttachmentsWithoutAnyMessageText();
    void testAcpBackendRaisesAgentPermissionRequests();
    void testAcpBackendSendsThePermissionAnswerToTheAgent();
    void testAcpBackendCancelsOutstandingPermissionRequests();

    void testTurnLedgerTracksTheActiveTurn();
    void testTurnLedgerDrainsPermissionsOnTurnEnd();
    void testLlmBackendStreamsThroughAFakeClient();
    void testRenderHistoryKeepsToolExchangePairs();
    void testLlmBackendIgnoresEventsFromOtherRequests();
    void testLlmBackendPermissionRoundTrip();
    void testLlmBackendDrainsPermissionsWhenTheTurnEnds();

    void testCoordinatorSwitchConfirmAndCancelRoundTrip();
    void testCoordinatorQuarantinesAndRestoresAgentBindings();
    void testCoordinatorRefusesIntentsWhileReadOnly();
    void testCoordinatorDefersTheSendForAutoCompress();
    void testCoordinatorGatesTheSummaryHandover();
    void testCoordinatorShrinkContextRoutesByBackendKind();

    void testAcpBackendTracksAgentSlashCommands();
    void testEarlyAgentCommandsSurviveEstablishment();
    void testAgentCommandInvocationSendsPlainPrompt();

    void testFileMentionSelectionFollowsChatToolsSetting();

    void testChatModelExposesSessionRowsDirectly();

    void testBlockCodecRejectsForgedMarkers();
    void testFileEditParsingRequiresTheMarkerAtTheStart();
    void testPermissionOptionsCarryAllowsAcrossTheSeam();

    void testChatFileStoreRoundTripsStoredContent();
    void testLegacyChatFileLoadsThroughTheFileStore();

    void testPickerObservesTheSharedAgentCatalog();
    void testAcpBackendSeedsAHandoverSummaryOnce();
    void testAcpBackendDropsAHandoverSummaryAfterACancelledTurn();
    void testAcpBackendDropsAHandoverSummaryWithTheConversation();
    void testToolSafetyDefaultsToMutating();
    void testToolsManagerWithoutAGateRunsImmediately();
    void testToolsManagerGateCanDeclineAToolCall();
    void testToolsManagerGateCanAllowAToolCall();
    void testToolsManagerGateResumesTheQueueAfterADenial();
    void testKnowledgeServerExposesOnlyReadOnlyTools();
    void testKnowledgeServerBindsLoopbackOnAnEphemeralPort();
    void testAcpBackendOffersKnowledgeToHttpCapableAgents();
    void testAcpBackendWithholdsKnowledgeFromAgentsWithoutHttpMcp();
    void testAcpBackendStopsTheKnowledgeServerWithTheSession();
    void testAcpBackendSurvivesAKnowledgeServerThatWillNotStart();
    void testAcpBackendResumesAPersistedSession();
    void testAcpBackendReportsAnUnresumableSession();
    void testAcpBackendRefusesToResumeWithoutAgentSupport();
    void testAcpBackendStartsFreshAfterAFailedResume();
    void testAgentBindingKeepsTheResumeTargetBeforeTheFirstTurn();
    void testAcpBackendCarriesTheLiveSessionIntoAResume();
    void testAcpBackendEstablishesTheSessionOnlyOnce();
    void testAgentBindingRejectsMalformedRecords();
    void testMalformedAgentBindingLeavesTheChatUnbound();
    void testAgentBindingRoundTripsThroughTheChatFile();
    void testChatFileWithoutAnAgentLoadsUnbound();
    void testAcpBackendMapsToolCallLifecycle();
    void testAcpBackendMapsPlanUpdates();
    void testAcpBackendReportsUsageFromThePromptResult();
    void testAcpBackendIgnoresTheCumulativeContextGauge();
    void testAcpBackendForwardsTheAgentSuggestedTitle();
    void testAcpBackendDeclinesAmbiguousPermissionOptions();
    void testAcpBackendClampsOversizedPermissionText();
    void testAcpBackendCancelsPermissionsWhenTheTurnCompletes();
    void testAcpBackendCancelsPermissionRequestsWhenTheTurnFails();

private:
    Context::DocumentContextReader createReader(const QString &text);
    QSharedPointer<Settings::CodeCompletionSettings> createSettingsForWholeFile();
    QSharedPointer<Settings::CodeCompletionSettings> createSettingsForLines(
        int linesBefore, int linesAfter);
    static QJsonObject expectedEphemeral(bool extendedTtl);
};

} // namespace QodeAssist
