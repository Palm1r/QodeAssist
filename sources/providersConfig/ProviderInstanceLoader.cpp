// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProviderInstanceLoader.hpp"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <toml++/toml.hpp>

#include <algorithm>
#include <sstream>

namespace QodeAssist::Providers {

namespace {

QJsonValue tomlToJson(const toml::node &node)
{
    if (auto *table = node.as_table()) {
        QJsonObject obj;
        for (const auto &[key, value] : *table) {
            obj.insert(QString::fromStdString(std::string{key.str()}), tomlToJson(value));
        }
        return obj;
    }
    if (auto *array = node.as_array()) {
        QJsonArray arr;
        for (const auto &item : *array)
            arr.append(tomlToJson(item));
        return arr;
    }
    if (auto *str = node.as_string())
        return QString::fromStdString(str->get());
    if (auto *integer = node.as_integer())
        return static_cast<qint64>(integer->get());
    if (auto *floating = node.as_floating_point())
        return floating->get();
    if (auto *boolean = node.as_boolean())
        return boolean->get();
    return QJsonValue::Null;
}

QJsonObject deepMerge(const QJsonObject &base, const QJsonObject &overlay)
{
    QJsonObject result = base;
    for (auto it = overlay.constBegin(); it != overlay.constEnd(); ++it) {
        const QJsonValue baseVal = result.value(it.key());
        const QJsonValue overlayVal = it.value();
        if (baseVal.isObject() && overlayVal.isObject())
            result[it.key()] = deepMerge(baseVal.toObject(), overlayVal.toObject());
        else
            result[it.key()] = overlayVal;
    }
    return result;
}

QString readUtf8(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Cannot open: %1").arg(path);
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

std::optional<QJsonObject> parseTomlFile(const QString &path, QString *error)
{
    QString readErr;
    const QString contents = readUtf8(path, &readErr);
    if (!readErr.isEmpty()) {
        if (error)
            *error = readErr;
        return std::nullopt;
    }
    toml::table tbl;
    try {
        tbl = toml::parse(contents.toStdString(), path.toStdString());
    } catch (const toml::parse_error &e) {
        std::ostringstream oss;
        oss << e;
        if (error) {
            *error = QStringLiteral("TOML parse error in %1: %2")
                         .arg(path, QString::fromStdString(oss.str()));
        }
        return std::nullopt;
    }
    return tomlToJson(tbl).toObject();
}

QStringList stringArray(const QJsonValue &v)
{
    QStringList out;
    if (!v.isArray())
        return out;
    for (const auto &elem : v.toArray()) {
        if (elem.isString())
            out.append(elem.toString());
    }
    return out;
}

LaunchConfig launchConfigFromObject(const QJsonObject &launchObj)
{
    LaunchConfig l;
    if (launchObj.isEmpty())
        return l;
    l.command = launchObj.value("command").toString();
    l.args = stringArray(launchObj.value("args"));
    l.cwd = launchObj.value("cwd").toString();
    const QJsonObject envObj = launchObj.value("env").toObject();
    for (auto it = envObj.constBegin(); it != envObj.constEnd(); ++it) {
        if (it.value().isString())
            l.env.insert(it.key(), it.value().toString());
    }
    l.readyUrl = launchObj.value("ready_url").toString();
    if (launchObj.contains("ready_timeout_s")) {
        const int raw = launchObj.value("ready_timeout_s")
                            .toInt(static_cast<int>(l.readyTimeout.count()));
        l.readyTimeout = std::chrono::seconds{std::max(1, raw)};
    }
    l.autoStart = launchObj.value("auto_start").toBool(false);
    l.detach = launchObj.value("detach").toBool(false);
    return l;
}

ProviderInstance instanceFromMerged(const QJsonObject &obj)
{
    ProviderInstance inst;
    inst.name        = obj.value("name").toString();
    inst.clientApi   = obj.value("client_api").toString();
    inst.description = obj.value("description").toString();
    inst.url         = obj.value("url").toString();
    inst.apiKeyRef   = obj.value("api_key_ref").toString();
    inst.extras      = obj.value("extras").toObject();
    inst.launch      = launchConfigFromObject(obj.value("launch").toObject());
    inst.extendsName = obj.value("extends").toString();
    inst.abstract    = obj.value("abstract").toBool(false);
    return inst;
}

struct RawEntry
{
    QJsonObject obj;
    QString filePath;
    bool overridesBundled = false;
};

constexpr int kMaxExtendsDepth = 16;

QJsonObject resolveExtends(
    const QString &name,
    const QHash<QString, RawEntry> &raw,
    QSet<QString> &visiting,
    QStringList &errors,
    int depth = 0)
{
    if (depth > kMaxExtendsDepth) {
        errors.append(QStringLiteral("Provider instance extends chain too deep (>%1) at '%2'")
                          .arg(kMaxExtendsDepth)
                          .arg(name));
        return {};
    }
    if (visiting.contains(name)) {
        errors.append(QStringLiteral("Cyclic 'extends' involving provider instance '%1'").arg(name));
        return {};
    }
    if (!raw.contains(name)) {
        errors.append(QStringLiteral("Unknown parent provider instance '%1'").arg(name));
        return {};
    }
    visiting.insert(name);

    QJsonObject self = raw.value(name).obj;
    const QString parent = self.value("extends").toString();
    if (!parent.isEmpty()) {
        const QJsonObject parentMerged
            = resolveExtends(parent, raw, visiting, errors, depth + 1);
        self.remove("extends");
        QJsonObject merged = deepMerge(parentMerged, self);
        merged["name"] = name;
        if (self.contains("abstract"))
            merged["abstract"] = self.value("abstract");
        else
            merged.remove("abstract");
        self = merged;
    }
    visiting.remove(name);
    return self;
}

} // namespace

std::optional<ProviderInstance> ProviderInstanceLoader::parseFile(
    const QString &path, QString *error)
{
    auto objOpt = parseTomlFile(path, error);
    if (!objOpt)
        return std::nullopt;
    ProviderInstance inst = instanceFromMerged(*objOpt);
    inst.sourcePath = path;
    return inst;
}

ProviderInstanceLoader::LoadResult ProviderInstanceLoader::load(
    const QString &qrcPrefix, const QString &userDir)
{
    LoadResult result;
    QHash<QString, RawEntry> raw;

    auto scan = [&](const QString &dir, bool isUserLayer) {
        if (dir.isEmpty())
            return;
        QDir d(dir);
        if (!d.exists())
            return;
        const QStringList files = d.entryList({"*.toml"}, QDir::Files);
        for (const QString &fname : files) {
            const QString fullPath = d.filePath(fname);
            QString err;
            auto objOpt = parseTomlFile(fullPath, &err);
            if (!objOpt) {
                result.errors.append(err);
                continue;
            }
            const QString name = objOpt->value("name").toString();
            if (name.isEmpty()) {
                result.errors.append(
                    QStringLiteral("Provider instance at %1 has no 'name'").arg(fullPath));
                continue;
            }
            const bool overrides = isUserLayer && raw.contains(name);
            raw.insert(name, {*objOpt, fullPath, overrides});
        }
    };

    scan(qrcPrefix, /*isUserLayer=*/false);
    scan(userDir,   /*isUserLayer=*/true);

    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
        const QString &name = it.key();

        QSet<QString> visiting;
        const QJsonObject merged = resolveExtends(name, raw, visiting, result.errors);
        if (merged.isEmpty())
            continue;

        ProviderInstance inst = instanceFromMerged(merged);
        inst.sourcePath = it.value().filePath;
        inst.overridesBundled = it.value().overridesBundled;

        if (inst.abstract)
            continue;
        result.instances.push_back(std::move(inst));
    }
    std::sort(result.instances.begin(), result.instances.end(),
              [](const ProviderInstance &a, const ProviderInstance &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    return result;
}

} // namespace QodeAssist::Providers
