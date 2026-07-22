// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class ToolsManagerGateTest final : public QObject
{
    Q_OBJECT

private slots:
    void testToolSafetyDefaultsToMutating();
    void testToolsManagerWithoutAGateRunsImmediately();
    void testToolsManagerGateCanDeclineAToolCall();
    void testToolsManagerGateCanAllowAToolCall();
    void testToolsManagerGateResumesTheQueueAfterADenial();
};

} // namespace QodeAssist
