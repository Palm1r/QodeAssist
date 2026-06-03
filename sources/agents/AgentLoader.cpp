// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentLoader.hpp"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

#include <toml++/toml.hpp>

#include <algorithm>
#include <sstream>

namespace QodeAssist::Agents {

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
        for (const auto &item : *array) {
            arr.append(tomlToJson(item));
        }
        return arr;
    }
    if (auto *str = node.as_string()) {
        return QString::fromStdString(str->get());
    }
    if (auto *integer = node.as_integer()) {
        return static_cast<qint64>(integer->get());
    }
    if (auto *floating = node.as_floating_point()) {
        return floating->get();
    }
    if (auto *boolean = node.as_boolean()) {
        return boolean->get();
    }
    return QJsonValue::Null;
}

QJsonObject deepMerge(const QJsonObject &base, const QJsonObject &overlay)
{
    QJsonObject result = base;
    for (auto it = overlay.constBegin(); it != overlay.constEnd(); ++it) {
        const QJsonValue baseVal = result.value(it.key());
        const QJsonValue overlayVal = it.value();
        if (baseVal.isObject() && overlayVal.isObject()) {
            result[it.key()] = deepMerge(baseVal.toObject(), overlayVal.toObject());
        } else {
            result[it.key()] = overlayVal;
        }
    }
    return result;
}

QString readUtf8(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open: %1").arg(path);
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

std::optional<QJsonObject> parseTomlFile(const QString &path, QString *error)
{
    QString readErr;
    const QString contents = readUtf8(path, &readErr);
    if (!readErr.isEmpty()) {
        if (error) *error = readErr;
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
    if (!v.isArray()) return out;
    for (const auto &elem : v.toArray()) {
        if (elem.isString()) out.append(elem.toString());
    }
    return out;
}

AgentConfig configFromMerged(const QJsonObject &obj)
{
    AgentConfig cfg;
    cfg.schemaVersion = obj.value("schema_version").toInt(1);
    cfg.name        = obj.value("name").toString();
    cfg.description = obj.value("description").toString();
    cfg.providerInstance = obj.value("provider_instance").toString();
    cfg.model       = obj.value("model").toString();
    cfg.endpoint    = obj.value("endpoint").toString();
    cfg.systemPrompt = obj.value("system_prompt").toString();
    cfg.enableThinking = obj.value("enable_thinking").toBool(false);
    cfg.enableTools    = obj.value("enable_tools").toBool(false);
    cfg.tags        = stringArray(obj.value("tags"));

    const QJsonObject matchObj = obj.value("match").toObject();
    cfg.match.filePatterns = stringArray(matchObj.value("file_patterns"));
    cfg.match.pathPatterns = stringArray(matchObj.value("path_patterns"));
    cfg.match.projectNames = stringArray(matchObj.value("project_names"));

    cfg.extendsName = obj.value("extends").toString();
    cfg.abstract    = obj.value("abstract").toBool(false);
    cfg.hidden      = obj.value("hidden").toBool(false);

    cfg.body = obj.value("body").toObject();
    return cfg;
}

struct RawEntry
{
    QJsonObject obj;
    QString filePath;
    bool overridesBundled = false;
};

constexpr int kMaxExtendsDepth = 32;

QJsonObject resolveExtends(
    const QString &name,
    const QHash<QString, RawEntry> &raw,
    QSet<QString> &visiting,
    QStringList &errors,
    int depth = 0)
{
    if (depth > kMaxExtendsDepth) {
        errors.append(QStringLiteral("Agent extends chain too deep (>%1) at '%2'")
                          .arg(kMaxExtendsDepth)
                          .arg(name));
        return {};
    }
    if (visiting.contains(name)) {
        errors.append(QStringLiteral("Cyclic 'extends' involving agent '%1'").arg(name));
        return {};
    }
    if (!raw.contains(name)) {
        errors.append(QStringLiteral("Unknown parent agent '%1'").arg(name));
        return {};
    }
    visiting.insert(name);

    QJsonObject self = raw.value(name).obj;
    const QString parent = self.value("extends").toString();
    if (!parent.isEmpty()) {
        const QJsonObject parentMerged
            = resolveExtends(parent, raw, visiting, errors, depth + 1);
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

std::optional<AgentConfig> AgentLoader::parseFile(
    const QString &path, QString *error, QStringList * /*warnings*/)
{
    auto objOpt = parseTomlFile(path, error);
    if (!objOpt) return std::nullopt;
    AgentConfig cfg = configFromMerged(*objOpt);
    cfg.sourcePath = path;
    return cfg;
}

AgentLoader::LoadResult AgentLoader::load(const QString &qrcPrefix, const QString &userDir)
{
    LoadResult result;
    QHash<QString, RawEntry> raw;

    auto scan = [&](const QString &dir, bool isUserLayer) {
        if (dir.isEmpty()) return;
        QDir d(dir);
        if (!d.exists()) return;
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
                result.errors.append(QStringLiteral("Agent at %1 has no 'name'").arg(fullPath));
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
        if (merged.isEmpty()) continue;

        AgentConfig cfg = configFromMerged(merged);
        cfg.sourcePath = it.value().filePath;
        cfg.overridesBundled = it.value().overridesBundled;

        if (cfg.abstract) continue;

        const QString validation = AgentConfig::validate(cfg);
        if (!validation.isEmpty()) {
            result.errors.append(validation);
            continue;
        }
        result.configs.push_back(std::move(cfg));
    }

    std::sort(result.configs.begin(), result.configs.end(),
              [](const AgentConfig &a, const AgentConfig &b) { return a.name < b.name; });
    return result;
}

} // namespace QodeAssist::Agents
