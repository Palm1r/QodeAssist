// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class LlmChatBackendTest final : public QObject
{
    Q_OBJECT

private slots:
    void testTurnLedgerTracksTheActiveTurn();
    void testTurnLedgerDrainsPermissionsOnTurnEnd();
    void testLlmBackendStreamsThroughAFakeClient();
    void testRenderHistoryKeepsToolExchangePairs();
    void testLlmBackendIgnoresEventsFromOtherRequests();
    void testLlmBackendPermissionRoundTrip();
    void testLlmBackendDrainsPermissionsWhenTheTurnEnds();
};

} // namespace QodeAssist
