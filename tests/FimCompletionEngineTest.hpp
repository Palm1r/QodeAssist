// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class FimCompletionEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void testFimEngineCompletesThroughTheProvider();
    void testFimEngineCancelIsPerRequest();
    void testFimEngineStripsCodeFences();
    void testFimEngineFailsWithoutDocument();
    void testFimEngineEnrichmentFollowsCompletionMode();
    void testFimEngineEnrichmentSkipsFimTemplates();
    void testIdentifiersNearCursorOrdersAndFilters();
    void testClampSectionsToTokenBudget();
    void testFimEngineFailsWithoutProvider();
};

} // namespace QodeAssist
