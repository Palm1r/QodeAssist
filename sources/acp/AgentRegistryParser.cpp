// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentRegistryParser.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace QodeAssist::Acp {

namespace {

QStringList readStringList(const QJsonValue &value)
{
    QStringList result;
    const QJsonArray array = value.toArray();
    for (const QJsonValue &entry : array) {
        if (entry.isString())
            result.append(entry.toString());
    }
    return result;
}

QList<AgentEnvVariable> readEnv(const QJsonValue &value)
{
    QList<AgentEnvVariable> result;

    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
            result.append({it.key(), it.value().toString()});
        return result;
    }

    const QJsonArray array = value.toArray();
    for (const QJsonValue &entry : array) {
        const QJsonObject object = entry.toObject();
        const QString name = object.value(QLatin1String("name")).toString();
        if (!name.isEmpty())
            result.append({name, object.value(QLatin1String("value")).toString()});
    }
    return result;
}

AgentDistribution readRunnerDistribution(const QJsonObject &object, AgentDistributionKind kind)
{
    AgentDistribution distribution;
    distribution.kind = kind;
    distribution.package = object.value(QLatin1String("package")).toString();
    distribution.args = readStringList(object.value(QLatin1String("args")));
    distribution.env = readEnv(object.value(QLatin1String("env")));
    return distribution;
}

AgentDistribution readBinaryDistribution(const QJsonObject &object)
{
    AgentDistribution distribution;
    distribution.kind = AgentDistributionKind::Binary;

    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        AgentBinaryTarget target;
        target.platform = it.key();
        target.archive = entry.value(QLatin1String("archive")).toString();
        target.sha256 = entry.value(QLatin1String("sha256")).toString();
        target.cmd = entry.value(QLatin1String("cmd")).toString();
        target.args = readStringList(entry.value(QLatin1String("args")));
        target.env = readEnv(entry.value(QLatin1String("env")));
        distribution.binaryTargets.append(target);
    }
    return distribution;
}

AgentDistribution readCommandDistribution(const QJsonObject &object)
{
    AgentDistribution distribution;
    distribution.kind = AgentDistributionKind::Command;
    distribution.command = object.value(QLatin1String("cmd")).toString();
    if (distribution.command.isEmpty())
        distribution.command = object.value(QLatin1String("command")).toString();
    distribution.args = readStringList(object.value(QLatin1String("args")));
    distribution.env = readEnv(object.value(QLatin1String("env")));
    return distribution;
}

QString distributionProblem(const AgentDistribution &distribution)
{
    switch (distribution.kind) {
    case AgentDistributionKind::Unknown:
        return QStringLiteral("has no supported distribution");
    case AgentDistributionKind::Npx:
    case AgentDistributionKind::Uvx:
        if (distribution.package.isEmpty())
            return QStringLiteral("has a %1 distribution without a package")
                .arg(agentDistributionName(distribution.kind));
        break;
    case AgentDistributionKind::Command:
        if (distribution.command.isEmpty())
            return QStringLiteral("has a command distribution without a cmd");
        break;
    case AgentDistributionKind::Binary:
        if (distribution.binaryTargets.isEmpty())
            return QStringLiteral("has a binary distribution without platform targets");
        break;
    }
    return {};
}

AgentDistribution readDistribution(const QJsonObject &object)
{
    if (object.contains(QLatin1String("npx")))
        return readRunnerDistribution(
            object.value(QLatin1String("npx")).toObject(), AgentDistributionKind::Npx);

    if (object.contains(QLatin1String("uvx")))
        return readRunnerDistribution(
            object.value(QLatin1String("uvx")).toObject(), AgentDistributionKind::Uvx);

    if (object.contains(QLatin1String("command")))
        return readCommandDistribution(object.value(QLatin1String("command")).toObject());

    if (object.contains(QLatin1String("binary")))
        return readBinaryDistribution(object.value(QLatin1String("binary")).toObject());

    return {};
}

} // namespace

namespace AgentRegistryParser {

AgentParseResult parse(const QJsonValue &root, AgentSource source, const QString &origin)
{
    AgentParseResult result;

    QJsonArray entries;
    if (root.isArray()) {
        entries = root.toArray();
    } else if (root.isObject()) {
        const QJsonObject object = root.toObject();
        if (object.contains(QLatin1String("agents"))) {
            const QJsonValue agents = object.value(QLatin1String("agents"));
            if (!agents.isArray()) {
                result.warnings.append(QStringLiteral("%1: 'agents' is not an array").arg(origin));
                return result;
            }
            entries = agents.toArray();
        } else if (object.contains(QLatin1String("id")))
            entries.append(root);
        else
            result.warnings.append(QStringLiteral("%1: no agents found").arg(origin));
    } else {
        result.warnings.append(QStringLiteral("%1: not a JSON object or array").arg(origin));
        return result;
    }

    int index = 0;
    for (const QJsonValue &entry : std::as_const(entries)) {
        const int position = index++;
        if (!entry.isObject()) {
            result.warnings.append(
                QStringLiteral("%1: entry %2 is not an object").arg(origin).arg(position));
            continue;
        }

        const QJsonObject object = entry.toObject();
        AgentDefinition agent;
        agent.id = object.value(QLatin1String("id")).toString();
        if (agent.id.isEmpty()) {
            result.warnings.append(
                QStringLiteral("%1: entry %2 has no id").arg(origin).arg(position));
            continue;
        }

        agent.name = object.value(QLatin1String("name")).toString();
        if (agent.name.isEmpty())
            agent.name = agent.id;

        agent.version = object.value(QLatin1String("version")).toString();
        agent.description = object.value(QLatin1String("description")).toString();
        agent.icon = object.value(QLatin1String("icon")).toString();
        agent.repository = object.value(QLatin1String("repository")).toString();
        agent.website = object.value(QLatin1String("website")).toString();
        agent.license = object.value(QLatin1String("license")).toString();
        agent.authors = readStringList(object.value(QLatin1String("authors")));
        agent.distribution = readDistribution(
            object.value(QLatin1String("distribution")).toObject());
        agent.source = source;
        agent.origin = origin;

        const QString problem = distributionProblem(agent.distribution);
        if (!problem.isEmpty()) {
            result.warnings.append(
                QStringLiteral("%1: agent '%2' %3").arg(origin, agent.id, problem));
        }

        result.agents.append(agent);
    }

    return result;
}

AgentParseResult parse(const QByteArray &json, AgentSource source, const QString &origin)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (document.isNull()) {
        AgentParseResult result;
        result.warnings.append(QStringLiteral("%1: %2").arg(origin, error.errorString()));
        return result;
    }

    if (document.isArray())
        return parse(QJsonValue(document.array()), source, origin);
    return parse(QJsonValue(document.object()), source, origin);
}

} // namespace AgentRegistryParser

} // namespace QodeAssist::Acp
