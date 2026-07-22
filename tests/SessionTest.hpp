// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class SessionTest final : public QObject
{
    Q_OBJECT

private slots:
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
};

} // namespace QodeAssist
