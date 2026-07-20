// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentSpawn.hpp"

#include <QCoreApplication>
#include <QDir>

#include <extensionsystem/pluginmanager.h>
#include <extensionsystem/pluginspec.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include <LLMQore/AcpClient.hpp>

#include "acp/AgentLaunch.hpp"
#include "settings/AgentsSettings.hpp"

namespace QodeAssist::Acp {

namespace {

LLMQore::Acp::Implementation clientIdentity()
{
    QString version;
    for (const ExtensionSystem::PluginSpec *spec : ExtensionSystem::PluginManager::plugins()) {
        if (spec->name() == QLatin1String("QodeAssist")) {
            version = spec->version();
            break;
        }
    }
    return {QStringLiteral("QodeAssist"), version, QStringLiteral("QodeAssist")};
}

} // namespace

QString agentWorkingDirectory()
{
    if (const ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject())
        return project->projectDirectory().path();
    return QDir::homePath();
}

AgentProcess spawnAgent(const AgentDefinition &agent, const QString &cwd, QObject *parent)
{
    auto config = agentLaunchConfig(agent, cwd);
    if (!config)
        return {};

    auto &settings = Settings::agentsSettings();
    applyExtraSearchPaths(*config, splitSearchPaths(settings.agentExtraPaths.volatileValue()));
    applyForwardedEnvironment(
        *config, splitVariableNames(settings.agentForwardedVariables.volatileValue()));

    auto *transport = config->createTransport(parent);
    return {new LLMQore::Acp::AcpClient(transport, clientIdentity(), parent), config->command};
}

QString runnerHint(const QString &command)
{
    if (command.endsWith(QLatin1String("npx"))) {
        return QCoreApplication::translate(
            "QtC::QodeAssist", "Make sure Node.js is installed and 'npx' is on PATH.");
    }
    if (command.endsWith(QLatin1String("uvx"))) {
        return QCoreApplication::translate(
            "QtC::QodeAssist", "Make sure uv is installed and 'uvx' is on PATH.");
    }
    return {};
}

} // namespace QodeAssist::Acp
