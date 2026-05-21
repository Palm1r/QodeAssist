// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProviderInstanceWriter.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>

#include "ProviderInstanceFactory.hpp"
#include "TomlWriter.hpp"

namespace QodeAssist::Providers {

namespace {

struct Tr
{
    Q_DECLARE_TR_FUNCTIONS(QtC::QodeAssist)
};

constexpr int kIdentityKeyColumn = 11; // longest key: "description"
constexpr int kLaunchKeyColumn = 15;   // longest key: "ready_timeout_s"

void writeIdentityBlock(TomlSerializer::TomlWriter &w, const ProviderInstance &inst)
{
    w.setKeyColumnWidth(0);
    w.writeInt(QStringLiteral("schema_version"), 1);
    w.writeBlankLine();

    w.setKeyColumnWidth(kIdentityKeyColumn);
    w.writeString(QStringLiteral("name"), inst.name);
    w.writeString(QStringLiteral("client_api"), inst.clientApi);
    if (!inst.description.isEmpty())
        w.writeString(QStringLiteral("description"), inst.description);
    w.writeBlankLine();
    w.writeString(QStringLiteral("url"), inst.url);
    if (!inst.apiKeyRef.isEmpty())
        w.writeString(QStringLiteral("api_key_ref"), inst.apiKeyRef);
}

void writeExtrasBlock(TomlSerializer::TomlWriter &w, const QJsonObject &extras)
{
    w.writeBlankLine();
    w.writeTableHeader(QStringLiteral("extras"));
    w.setKeyColumnWidth(0);
    w.writeJsonPrimitives(extras);
}

void writeLaunchBlock(TomlSerializer::TomlWriter &w, const LaunchConfig &l)
{
    w.writeBlankLine();
    w.writeTableHeader(QStringLiteral("launch"));
    w.setKeyColumnWidth(kLaunchKeyColumn);
    w.writeString(QStringLiteral("command"), l.command);
    if (!l.args.isEmpty())
        w.writeStringArray(QStringLiteral("args"), l.args);
    if (!l.cwd.isEmpty())
        w.writeString(QStringLiteral("cwd"), l.cwd);
    if (!l.readyUrl.isEmpty())
        w.writeString(QStringLiteral("ready_url"), l.readyUrl);
    w.writeInt(QStringLiteral("ready_timeout_s"), l.readyTimeout.count());
    w.writeBool(QStringLiteral("auto_start"), l.autoStart);
    w.writeBool(QStringLiteral("detach"), l.detach);

    if (!l.env.isEmpty()) {
        w.writeBlankLine();
        w.writeTableHeader(QStringLiteral("launch.env"));
        w.setKeyColumnWidth(0);
        w.writeStringDict(l.env);
    }
}

} // namespace

QString ProviderInstanceWriter::toToml(const ProviderInstance &inst)
{
    TomlSerializer::TomlWriter w;
    writeIdentityBlock(w, inst);
    if (!inst.extras.isEmpty())
        writeExtrasBlock(w, inst.extras);
    if (!inst.launch.isEmpty())
        writeLaunchBlock(w, inst.launch);
    return w.result();
}

QString ProviderInstanceWriter::deriveBaseName(const QString &name)
{
    QString baseName;
    for (QChar c : name) {
        if (c.isLetterOrNumber())
            baseName.append(c.toLower());
        else if (c == QLatin1Char(' ') || c == QLatin1Char('-') || c == QLatin1Char('_'))
            baseName.append(QLatin1Char('_'));
    }
    while (baseName.startsWith(QLatin1Char('_')))
        baseName.remove(0, 1);
    while (baseName.endsWith(QLatin1Char('_')))
        baseName.chop(1);
    if (baseName.isEmpty())
        baseName = QStringLiteral("instance");
    return baseName;
}

namespace {
constexpr int kMaxCollisionRetries = 1000;
} // namespace

QString ProviderInstanceWriter::pickUserFilePath(
    const QString &userDir, const QString &name, const QString &previousPath)
{
    const QDir dir(userDir);
    const QString base = deriveBaseName(name);
    const QString preferred = dir.filePath(base + QLatin1String(".toml"));
    if (!previousPath.isEmpty()
        && QFileInfo(previousPath).absolutePath() == dir.absolutePath()
        && QFileInfo(previousPath).absoluteFilePath() == QFileInfo(preferred).absoluteFilePath())
        return preferred;
    QSet<QString> taken;
    for (const QString &existing : dir.entryList({"*.toml"}, QDir::Files))
        taken.insert(existing);
    if (!taken.contains(base + QLatin1String(".toml")))
        return preferred;
    for (int i = 2; i < kMaxCollisionRetries; ++i) {
        const QString candidate = QStringLiteral("%1_%2.toml").arg(base).arg(i);
        if (!taken.contains(candidate))
            return dir.filePath(candidate);
    }
    return {};
}

QString ProviderInstanceWriter::writeToUserDir(
    const ProviderInstance &inst, const QString &previousPath, QString *errorOut)
{
    const QString userDir = ProviderInstanceFactory::userInstancesDir();
    if (!QDir().mkpath(userDir)) {
        if (errorOut)
            *errorOut = Tr::tr("Cannot create user provider folder:\n%1").arg(userDir);
        return {};
    }
    const QString filePath = pickUserFilePath(userDir, inst.name, previousPath);
    if (filePath.isEmpty()) {
        if (errorOut)
            *errorOut = Tr::tr("Cannot pick a free filename in:\n%1").arg(userDir);
        return {};
    }
    QSaveFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorOut)
            *errorOut = Tr::tr("Cannot write %1:\n%2").arg(filePath, f.errorString());
        return {};
    }
    const QByteArray bytes = toToml(inst).toUtf8();
    if (f.write(bytes) != bytes.size() || !f.commit()) {
        if (errorOut)
            *errorOut = Tr::tr("Write failed for %1:\n%2").arg(filePath, f.errorString());
        return {};
    }
    return filePath;
}

} // namespace QodeAssist::Providers
