// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QString>

#include "acp/AgentDefinition.hpp"

namespace LLMQore::Acp {
class AcpClient;
}

namespace QodeAssist::Acp {

struct AgentProcess
{
    LLMQore::Acp::AcpClient *client = nullptr;
    QString command;
};

QString agentWorkingDirectory();

AgentProcess spawnAgent(const AgentDefinition &agent, const QString &cwd, QObject *parent);

QString runnerHint(const QString &command);

} // namespace QodeAssist::Acp
