// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QByteArray>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

#include "acp/AgentDefinition.hpp"

namespace QodeAssist::Acp {

struct AgentParseResult
{
    QList<AgentDefinition> agents;
    QStringList warnings;
};

namespace AgentRegistryParser {

AgentParseResult parse(const QJsonValue &root, AgentSource source, const QString &origin);
AgentParseResult parse(const QByteArray &json, AgentSource source, const QString &origin);

} // namespace AgentRegistryParser

} // namespace QodeAssist::Acp
