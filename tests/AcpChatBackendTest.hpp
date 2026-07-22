// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class AcpChatBackendTest final : public QObject
{
    Q_OBJECT

private slots:
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
    void testAcpBackendSeedsAHandoverSummaryOnce();
    void testAcpBackendDropsAHandoverSummaryAfterACancelledTurn();
    void testAcpBackendDropsAHandoverSummaryWithTheConversation();
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
    void testAcpBackendMapsToolCallLifecycle();
    void testAcpBackendMapsPlanUpdates();
    void testAcpBackendReportsUsageFromThePromptResult();
    void testAcpBackendIgnoresTheCumulativeContextGauge();
    void testAcpBackendForwardsTheAgentSuggestedTitle();
    void testAcpBackendDeclinesAmbiguousPermissionOptions();
    void testAcpBackendClampsOversizedPermissionText();
    void testAcpBackendCancelsPermissionsWhenTheTurnCompletes();
    void testAcpBackendCancelsPermissionRequestsWhenTheTurnFails();
    void testAcpBackendTracksAgentSlashCommands();
    void testEarlyAgentCommandsSurviveEstablishment();
    void testAgentCommandInvocationSendsPlainPrompt();
};

} // namespace QodeAssist
