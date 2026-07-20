// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentDefinition.hpp"

#include <QSysInfo>

namespace QodeAssist::Acp {

bool AgentDefinition::isLaunchable() const
{
    switch (distribution.kind) {
    case AgentDistributionKind::Npx:
    case AgentDistributionKind::Uvx:
        return !distribution.package.isEmpty();
    case AgentDistributionKind::Command:
        return !distribution.command.isEmpty();
    case AgentDistributionKind::Binary:
    case AgentDistributionKind::Unknown:
        return false;
    }
    return false;
}

QString agentSourceName(AgentSource source)
{
    switch (source) {
    case AgentSource::UserFile:
        return QStringLiteral("user file");
    case AgentSource::LiveRegistry:
        return QStringLiteral("registry");
    case AgentSource::BundledSnapshot:
        return QStringLiteral("bundled");
    }
    return {};
}

QString agentDistributionName(AgentDistributionKind kind)
{
    switch (kind) {
    case AgentDistributionKind::Npx:
        return QStringLiteral("npx");
    case AgentDistributionKind::Uvx:
        return QStringLiteral("uvx");
    case AgentDistributionKind::Binary:
        return QStringLiteral("binary");
    case AgentDistributionKind::Command:
        return QStringLiteral("command");
    case AgentDistributionKind::Unknown:
        return QStringLiteral("unknown");
    }
    return {};
}

QString currentBinaryPlatform()
{
    const QString kernel = QSysInfo::kernelType();
    QString os;
    if (kernel == QLatin1String("darwin"))
        os = QStringLiteral("darwin");
    else if (kernel == QLatin1String("linux"))
        os = QStringLiteral("linux");
    else if (kernel == QLatin1String("winnt"))
        os = QStringLiteral("windows");
    else
        return {};

    const QString cpu = QSysInfo::currentCpuArchitecture();
    QString arch;
    if (cpu == QLatin1String("arm64") || cpu == QLatin1String("aarch64"))
        arch = QStringLiteral("aarch64");
    else if (cpu == QLatin1String("x86_64"))
        arch = QStringLiteral("x86_64");
    else
        return {};

    return os + QLatin1Char('-') + arch;
}

const AgentBinaryTarget *binaryTargetForCurrentPlatform(const AgentDefinition &agent)
{
    const QString platform = currentBinaryPlatform();
    if (platform.isEmpty())
        return nullptr;

    for (const AgentBinaryTarget &target : agent.distribution.binaryTargets) {
        if (target.platform == platform)
            return &target;
    }
    return nullptr;
}

} // namespace QodeAssist::Acp
