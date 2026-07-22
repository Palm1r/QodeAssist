// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class ConversationCoordinatorTest final : public QObject
{
    Q_OBJECT

private slots:
    void testCoordinatorSwitchConfirmAndCancelRoundTrip();
    void testCoordinatorQuarantinesAndRestoresAgentBindings();
    void testCoordinatorRefusesIntentsWhileReadOnly();
    void testCoordinatorDefersTheSendForAutoCompress();
    void testCoordinatorGatesTheSummaryHandover();
    void testCoordinatorShrinkContextRoutesByBackendKind();
};

} // namespace QodeAssist
