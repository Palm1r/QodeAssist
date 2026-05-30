// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ContextRenderer.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <filesystem>
#include <stdexcept>

#include <inja/inja.hpp>

namespace QodeAssist::Templates::ContextRenderer {

namespace {

QString substituteVars(const QString &src, const Bindings &b)
{
    QString out = src;
    if (!b.projectDir.isEmpty())
        out.replace(QStringLiteral("${PROJECT_DIR}"), b.projectDir);
    if (!b.configDir.isEmpty())
        out.replace(QStringLiteral("${CONFIG_DIR}"), b.configDir);
    return out;
}

bool isPathAllowed(const QString &requestedPath, const Bindings &b)
{
    if (requestedPath.startsWith(QLatin1String(":/")))
        return true;

    const QString target = QDir::cleanPath(requestedPath);

    auto isUnder = [&target](const QString &root) {
        if (root.isEmpty())
            return false;
        const QString cleanRoot = QDir::cleanPath(root);
        if (target == cleanRoot)
            return true;
        return target.startsWith(cleanRoot + QLatin1Char('/'));
    };

    if (isUnder(b.projectDir))
        return true;
    if (isUnder(b.configDir))
        return true;
    return false;
}

QString expandAndResolvePath(const QString &raw, const Bindings &b)
{
    QString p = raw;
    if (!b.projectDir.isEmpty())
        p.replace(QStringLiteral("${PROJECT_DIR}"), b.projectDir);
    if (!b.configDir.isEmpty())
        p.replace(QStringLiteral("${CONFIG_DIR}"), b.configDir);
    return p;
}

[[noreturn]] void throwOutsideRoots(const char *fn, const QString &path)
{
    throw std::runtime_error(
        QStringLiteral("%1: path is outside the allowed read roots "
                       "(the project directory, ~/qodeassist, or bundled :/ resources): %2")
            .arg(QString::fromLatin1(fn), path)
            .toStdString());
}

void registerReadFile(inja::Environment &env, const Bindings &b)
{
    const Bindings caps = b;
    env.add_callback("read_file", 1, [caps](inja::Arguments &args) -> nlohmann::json {
        const QString path
            = expandAndResolvePath(QString::fromStdString(args.at(0)->get<std::string>()), caps);

        if (!isPathAllowed(path, caps))
            throwOutsideRoots("read_file", path);

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            throw std::runtime_error(
                QStringLiteral("read_file: cannot open file (missing or unreadable): %1")
                    .arg(path)
                    .toStdString());
        }
        return f.readAll().toStdString();
    });
}

void registerFileExists(inja::Environment &env, const Bindings &b)
{
    const Bindings caps = b;
    env.add_callback("file_exists", 1, [caps](inja::Arguments &args) -> nlohmann::json {
        const QString p
            = expandAndResolvePath(QString::fromStdString(args.at(0)->get<std::string>()), caps);
        if (!isPathAllowed(p, caps))
            throwOutsideRoots("file_exists", p);
        return QFileInfo::exists(p);
    });
}

void registerReadDir(inja::Environment &env, const Bindings &b)
{
    const Bindings caps = b;

    env.add_callback("read_dir", 1, [caps](inja::Arguments &args) -> nlohmann::json {
        const QString p
            = expandAndResolvePath(QString::fromStdString(args.at(0)->get<std::string>()), caps);
        if (!isPathAllowed(p, caps))
            throwOutsideRoots("read_dir", p);
        QDir dir(p);
        if (!dir.exists())
            return nlohmann::json::array();
        nlohmann::json out = nlohmann::json::array();
        const QStringList entries
            = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &name : entries)
            out.push_back(name.toStdString());
        return out;
    });
}

void registerStringHelpers(inja::Environment &env)
{
    env.add_callback("head_lines", 2, [](inja::Arguments &args) -> nlohmann::json {
        const QString text = QString::fromStdString(args.at(0)->get<std::string>());
        const int n = args.at(1)->get<int>();
        if (n <= 0)
            return std::string{};
        const QStringList lines = text.split('\n');
        const int take = std::min<int>(n, lines.size());
        QStringList head;
        head.reserve(take);
        for (int i = 0; i < take; ++i)
            head.append(lines.at(i));
        return head.join('\n').toStdString();
    });

    env.add_callback("basename", 1, [](inja::Arguments &args) -> nlohmann::json {
        return QFileInfo(QString::fromStdString(args.at(0)->get<std::string>()))
            .fileName()
            .toStdString();
    });
    env.add_callback("dirname", 1, [](inja::Arguments &args) -> nlohmann::json {
        return QFileInfo(QString::fromStdString(args.at(0)->get<std::string>())).path().toStdString();
    });
    env.add_callback("ext", 1, [](inja::Arguments &args) -> nlohmann::json {
        return QFileInfo(QString::fromStdString(args.at(0)->get<std::string>()))
            .suffix()
            .toStdString();
    });

    env.add_callback("lower", 1, [](inja::Arguments &args) -> nlohmann::json {
        return QString::fromStdString(args.at(0)->get<std::string>()).toLower().toStdString();
    });
    env.add_callback("upper", 1, [](inja::Arguments &args) -> nlohmann::json {
        return QString::fromStdString(args.at(0)->get<std::string>()).toUpper().toStdString();
    });
}

void registerSandbox(inja::Environment &env)
{
    env.set_search_included_templates_in_files(false);
    env.set_include_callback(
        [](const std::filesystem::path &, const std::string &name) -> inja::Template {
            throw inja::FileError("include is disabled in QodeAssist context: '" + name + "'");
        });

    env.set_line_statement("@@@inja@@@");
}

} // namespace

QString render(const QString &templateSource, const Bindings &bindings, QString *error)
{
    if (templateSource.isEmpty())
        return {};

    const QString substituted = substituteVars(templateSource, bindings);

    inja::Environment env;
    registerSandbox(env);
    registerReadFile(env, bindings);
    registerFileExists(env, bindings);
    registerReadDir(env, bindings);
    registerStringHelpers(env);

    inja::Template tpl;
    try {
        tpl = env.parse(substituted.toStdString());
    } catch (const std::exception &e) {
        if (error) {
            *error = QStringLiteral("Failed to parse context jinja: %1")
                         .arg(QString::fromUtf8(e.what()));
        }
        return {};
    }

    try {
        nlohmann::json data = nlohmann::json::object();
        data["language"] = bindings.language.toStdString();
        const std::string rendered = env.render(tpl, data);
        return QString::fromStdString(rendered);
    } catch (const std::exception &e) {
        if (error) {
            *error = QStringLiteral("Failed to render context jinja: %1")
                         .arg(QString::fromUtf8(e.what()));
        }
        return {};
    }
}

} // namespace QodeAssist::Templates::ContextRenderer
