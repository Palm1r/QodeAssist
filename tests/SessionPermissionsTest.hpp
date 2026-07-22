// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class SessionPermissionsTest final : public QObject
{
    Q_OBJECT

private slots:
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
};

} // namespace QodeAssist
