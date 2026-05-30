// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentDuplicator.hpp"

#include <Agent.hpp>
#include <AgentConfig.hpp>
#include <AgentFactory.hpp>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace QodeAssist::Settings {

namespace {

QString tomlEscape(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s) {
        switch (c.unicode()) {
        case '\\':
            out += QLatin1String("\\\\");
            break;
        case '"':
            out += QLatin1String("\\\"");
            break;
        case '\n':
            out += QLatin1String("\\n");
            break;
        case '\r':
            out += QLatin1String("\\r");
            break;
        case '\t':
            out += QLatin1String("\\t");
            break;
        default:
            out += c;
        }
    }
    return out;
}

constexpr int kMaxUniqueAttempts = 1000;

QString uniqueFilename(const QString &userDir, const QString &parentBasename)
{
    QString fileName = parentBasename + QStringLiteral("_custom.toml");
    for (int n = 2; n < kMaxUniqueAttempts && QFile::exists(QDir(userDir).filePath(fileName)); ++n)
        fileName = QStringLiteral("%1_custom_%2.toml").arg(parentBasename).arg(n);
    return QDir(userDir).filePath(fileName);
}

QString uniqueName(const QString &parentName, const AgentFactory &factory)
{
    QString newName = QStringLiteral("%1 (Custom)").arg(parentName);
    for (int n = 2; n < kMaxUniqueAttempts && factory.configByName(newName); ++n)
        newName = QStringLiteral("%1 (Custom %2)").arg(parentName).arg(n);
    return newName;
}

QString trUser(const char *src)
{
    return QCoreApplication::translate("QodeAssist::Settings::AgentDuplicator", src);
}

} // namespace

AgentDuplicateResult duplicateAgentInUserDir(const AgentConfig &parent, const AgentFactory &factory)
{
    AgentDuplicateResult result;
    if (parent.name.trimmed().isEmpty()) {
        result.error = trUser("Parent agent has no name; cannot duplicate.");
        return result;
    }

    const QString userDir = AgentFactory::userAgentsDir();
    if (!QDir().mkpath(userDir)) {
        result.error = trUser("Cannot create user agents folder: %1").arg(userDir);
        return result;
    }

    const QString parentBasename = QFileInfo(parent.sourcePath).baseName();
    result.filePath = uniqueFilename(userDir, parentBasename);
    if (QFile::exists(result.filePath)) {
        result.error
            = trUser("Could not find a free filename after %1 attempts.").arg(kMaxUniqueAttempts);
        return result;
    }
    result.newName = uniqueName(parent.name, factory);
    if (factory.configByName(result.newName)) {
        result.error
            = trUser("Could not find a free agent name after %1 attempts.").arg(kMaxUniqueAttempts);
        return result;
    }

    QSaveFile f(result.filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.error = trUser("Cannot create %1: %2").arg(result.filePath, f.errorString());
        return result;
    }
    const QString description = QStringLiteral(
                                    "User customization of '%1'. Override fields below to taste; "
                                    "values not overridden are inherited from the parent.")
                                    .arg(parent.name);
    QString body
        = QStringLiteral(
              "schema_version = 1\n"
              "name = \"%1\"\n"
              "extends = \"%2\"\n"
              "description = \"%3\"\n")
              .arg(tomlEscape(result.newName), tomlEscape(parent.name), tomlEscape(description));
    const QByteArray payload = body.toUtf8();
    if (f.write(payload) != payload.size() || !f.commit()) {
        result.error = trUser("Failed to write %1: %2").arg(result.filePath, f.errorString());
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace QodeAssist::Settings
