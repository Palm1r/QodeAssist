// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class AgentCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
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
    void testPickerObservesTheSharedAgentCatalog();
};

} // namespace QodeAssist
