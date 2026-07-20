// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <optional>

#include <QString>

#include <LLMQore/AcpAgentConfig.hpp>

#include "acp/AgentDefinition.hpp"

namespace QodeAssist::Acp {

std::optional<LLMQore::Acp::AcpAgentConfig> agentLaunchConfig(
    const AgentDefinition &agent, const QString &workingDirectory);

QStringList splitSearchPaths(const QString &value);

void applyExtraSearchPaths(LLMQore::Acp::AcpAgentConfig &config, const QStringList &extraDirectories);

QStringList splitVariableNames(const QString &value);

void applyForwardedEnvironment(
    LLMQore::Acp::AcpAgentConfig &config, const QStringList &variableNames);

} // namespace QodeAssist::Acp
