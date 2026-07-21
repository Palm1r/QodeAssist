// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace QodeAssist::Acp {

enum class AgentSource { BundledSnapshot, LiveRegistry, UserFile };

inline constexpr int agentSourceCount = 3;

enum class AgentDistributionKind { Unknown, Npx, Uvx, Binary, Command };

struct AgentEnvVariable
{
    QString name;
    QString value;
};

struct AgentBinaryTarget
{
    QString platform;
    QString archive;
    QString sha256;
    QString cmd;
    QStringList args;
    QList<AgentEnvVariable> env;
};

struct AgentDistribution
{
    AgentDistributionKind kind = AgentDistributionKind::Unknown;
    QString package;
    QString command;
    QStringList args;
    QList<AgentEnvVariable> env;
    QList<AgentBinaryTarget> binaryTargets;
};

struct AgentDefinition
{
    QString id;
    QString name;
    QString version;
    QString description;
    QString icon;
    QString repository;
    QString website;
    QString license;
    QStringList authors;
    AgentDistribution distribution;
    AgentSource source = AgentSource::BundledSnapshot;
    QString origin;

    bool isLaunchable() const;
};

QString agentSourceName(AgentSource source);
QString agentDistributionName(AgentDistributionKind kind);

QString currentBinaryPlatform();
const AgentBinaryTarget *binaryTargetForCurrentPlatform(const AgentDefinition &agent);

} // namespace QodeAssist::Acp
