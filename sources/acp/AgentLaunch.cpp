// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentLaunch.hpp"

#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

namespace QodeAssist::Acp {

namespace {

QList<LLMQore::Acp::EnvVariable> toLlmqoreEnv(const QList<AgentEnvVariable> &env)
{
    QList<LLMQore::Acp::EnvVariable> result;
    result.reserve(env.size());
    for (const AgentEnvVariable &variable : env)
        result.append({variable.name, variable.value});
    return result;
}

void applyRunner(
    LLMQore::Acp::AcpAgentConfig &config,
    const QString &runner,
    const QStringList &runnerFlags,
    const AgentDistribution &distribution)
{
    config.command = runner;
    config.args = runnerFlags;
    config.args.append(distribution.package);
    config.args.append(distribution.args);
}

QProcessEnvironment harvestLoginShellEnvironment()
{
    QProcessEnvironment result;

#ifndef Q_OS_WIN
    static const QRegularExpression assignment(QStringLiteral("^([A-Za-z_][A-Za-z0-9_]*)=(.*)$"));

    const QString shell = qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh"));

    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(
        shell,
        {QStringLiteral("-l"), QStringLiteral("-i"), QStringLiteral("-c"), QStringLiteral("env")});

    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        return result;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QList<QString> lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = assignment.match(line);
        if (match.hasMatch())
            result.insert(match.captured(1), match.captured(2));
    }
#endif

    return result;
}

const QProcessEnvironment &loginShellEnvironment()
{
    static const QProcessEnvironment environment = harvestLoginShellEnvironment();
    return environment;
}

} // namespace

std::optional<LLMQore::Acp::AcpAgentConfig> agentLaunchConfig(
    const AgentDefinition &agent, const QString &workingDirectory)
{
    if (!agent.isLaunchable())
        return std::nullopt;

    LLMQore::Acp::AcpAgentConfig config;
    config.cwd = workingDirectory;
    config.env = toLlmqoreEnv(agent.distribution.env);

    switch (agent.distribution.kind) {
    case AgentDistributionKind::Npx:
        applyRunner(
            config, QStringLiteral("npx"), QStringList{QStringLiteral("-y")}, agent.distribution);
        break;
    case AgentDistributionKind::Uvx:
        applyRunner(config, QStringLiteral("uvx"), {}, agent.distribution);
        break;
    case AgentDistributionKind::Command:
        config.command = agent.distribution.command;
        config.args = agent.distribution.args;
        break;
    case AgentDistributionKind::Binary:
    case AgentDistributionKind::Unknown:
        return std::nullopt;
    }

    return config;
}

QStringList splitSearchPaths(const QString &value)
{
    QStringList directories;
    const QList<QString> entries = value.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const QString trimmed = entry.trimmed();
        if (!trimmed.isEmpty())
            directories.append(trimmed);
    }
    return directories;
}

void applyExtraSearchPaths(LLMQore::Acp::AcpAgentConfig &config, const QStringList &extraDirectories)
{
    if (extraDirectories.isEmpty())
        return;

    const bool isBareName = !config.command.isEmpty() && !config.command.contains(QLatin1Char('/'))
                            && !config.command.contains(QLatin1Char('\\'));
    if (isBareName) {
        const QString resolved = QStandardPaths::findExecutable(config.command, extraDirectories);
        if (!resolved.isEmpty())
            config.command = resolved;
    }

    const QChar separator = QDir::listSeparator();
    const QString inherited = QProcessEnvironment::systemEnvironment().value(QStringLiteral("PATH"));
    const QString merged = inherited.isEmpty()
                               ? extraDirectories.join(separator)
                               : extraDirectories.join(separator) + separator + inherited;

    config.env.prepend({QStringLiteral("PATH"), merged});
}

QStringList splitVariableNames(const QString &value)
{
    static const QRegularExpression separators(QStringLiteral("[,;\\s]+"));

    QStringList names;
    const QList<QString> entries = value.split(separators, Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const QString trimmed = entry.trimmed();
        if (!trimmed.isEmpty())
            names.append(trimmed);
    }
    return names;
}

void applyForwardedEnvironment(LLMQore::Acp::AcpAgentConfig &config, const QStringList &variableNames)
{
    if (variableNames.isEmpty())
        return;

    const QProcessEnvironment inherited = QProcessEnvironment::systemEnvironment();

    QList<LLMQore::Acp::EnvVariable> forwarded;
    QStringList missing;
    for (const QString &name : variableNames) {
        if (inherited.contains(name))
            forwarded.append({name, inherited.value(name)});
        else
            missing.append(name);
    }

    if (!missing.isEmpty()) {
        const QProcessEnvironment &shell = loginShellEnvironment();
        for (const QString &name : std::as_const(missing)) {
            if (shell.contains(name))
                forwarded.append({name, shell.value(name)});
        }
    }

    for (auto it = forwarded.crbegin(); it != forwarded.crend(); ++it)
        config.env.prepend(*it);
}

} // namespace QodeAssist::Acp
