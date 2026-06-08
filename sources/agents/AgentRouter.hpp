// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>
#include <QStringList>

#include "AgentConfig.hpp"

namespace QodeAssist {

class AgentFactory;

namespace AgentRouter {

struct Context
{
    QString filePath;
    QString projectName;
};

[[nodiscard]] bool matches(const AgentConfig::Match &match, const Context &ctx);

[[nodiscard]] QString pickAgent(
    const QStringList &roster, const Context &ctx, const AgentFactory &factory);

} // namespace AgentRouter

} // namespace QodeAssist
