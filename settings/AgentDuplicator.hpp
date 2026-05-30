// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

namespace QodeAssist {
class AgentFactory;
struct AgentConfig;
} // namespace QodeAssist

namespace QodeAssist::Settings {

struct AgentDuplicateResult
{
    bool ok = false;
    QString filePath;
    QString newName;
    QString error;
};

AgentDuplicateResult duplicateAgentInUserDir(const AgentConfig &parent, const AgentFactory &factory);

} // namespace QodeAssist::Settings
