// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ContextRenderer.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <coreplugin/icore.h>

#include <filesystem>

#include <inja/inja.hpp>

namespace QodeAssist::Templates::ContextRenderer {

namespace {

QString substituteVars(const QString &src, const Bindings &b)
{
    QString out = src;
    if (!b.projectDir.isEmpty())
        out.replace(QStringLiteral("${PROJECT_DIR}"), b.projectDir);
    if (!b.homeDir.isEmpty())
        out.replace(QStringLiteral("${HOME}"), b.homeDir);
    return out;
}

bool isPathAllowed(const QString &requestedPath, const Bindings &b)
{
    const QString target = QDir::cleanPath(requestedPath);

    auto isUnder = [&target](const QString &root) {
        if (root.isEmpty()) return false;
        const QString cleanRoot = QDir::cleanPath(root);
        if (target == cleanRoot) return true;
        return target.startsWith(cleanRoot + QLatin1Char('/'));
    };

    if (isUnder(b.projectDir)) return true;
    if (!b.homeDir.isEmpty() && isUnder(b.homeDir + QStringLiteral("/qodeassist")))
        return true;
    return false;
}

void registerReadFile(inja::Environment &env, const Bindings &b)
{
    const Bindings capturedBindings = b;
    env.add_callback("read_file", 1, [capturedBindings](inja::Arguments &args) -> nlohmann::json {
        const std::string raw = args.at(0)->get<std::string>();
        QString path = QString::fromStdString(raw);
        
        if (!capturedBindings.projectDir.isEmpty())
            path.replace(QStringLiteral("${PROJECT_DIR}"), capturedBindings.projectDir);
        if (!capturedBindings.homeDir.isEmpty())
            path.replace(QStringLiteral("${HOME}"), capturedBindings.homeDir);

        if (!isPathAllowed(path, capturedBindings)) {
            qWarning("[QodeAssist] context.read_file: path not in allowed roots: %s",
                     qUtf8Printable(path));
            return std::string{};
        }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return std::string{};
        return f.readAll().toStdString();
    });
}

QString expandAndResolvePath(const QString &raw, const Bindings &b)
{
    QString p = raw;
    if (!b.projectDir.isEmpty())
        p.replace(QStringLiteral("${PROJECT_DIR}"), b.projectDir);
    if (!b.homeDir.isEmpty())
        p.replace(QStringLiteral("${HOME}"), b.homeDir);
    return p;
}

void registerFileExists(inja::Environment &env, const Bindings &b)
{
    const Bindings caps = b;
    env.add_callback("file_exists", 1, [caps](inja::Arguments &args) -> nlohmann::json {
        const QString p = expandAndResolvePath(
            QString::fromStdString(args.at(0)->get<std::string>()), caps);
        if (!isPathAllowed(p, caps))
            return false;
        return QFileInfo::exists(p);
    });
}

void registerReadDir(inja::Environment &env, const Bindings &b)
{
    const Bindings caps = b;
 
    env.add_callback("read_dir", 1, [caps](inja::Arguments &args) -> nlohmann::json {
        const QString p = expandAndResolvePath(
            QString::fromStdString(args.at(0)->get<std::string>()), caps);
        if (!isPathAllowed(p, caps)) {
            qWarning("[QodeAssist] context.read_dir: path not in allowed roots: %s",
                     qUtf8Printable(p));
            return nlohmann::json::array();
        }
        QDir dir(p);
        if (!dir.exists())
            return nlohmann::json::array();
        nlohmann::json out = nlohmann::json::array();
        const QStringList entries = dir.entryList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
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
        return QFileInfo(QString::fromStdString(args.at(0)->get<std::string>()))
            .path()
            .toStdString();
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

// Read a role's system prompt from the role JSON written by the settings Roles
// UI (AgentRolesManager). Returns "" if the role doesn't exist.
std::string roleSystemPrompt(const QString &id)
{
    if (id.isEmpty())
        return {};
    const QString path
        = Core::ICore::userResourcePath(
              QStringLiteral("qodeassist/agent_roles/%1.json").arg(id))
              .toFSPathString();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("[QodeAssist] agent_role: role '%s' not found at %s",
                 qUtf8Printable(id), qUtf8Printable(path));
        return {};
    }
    return QJsonDocument::fromJson(f.readAll())
        .object()
        .value("systemPrompt")
        .toString()
        .toStdString();
}

// Building blocks for composing a profile's `system_prompt` (alongside
// read_file/file_exists):
//   {{ agent_role() }}      — the runtime-selected role (Bindings.roleId, which
//                             the chat sets; falls back to "developer")
//   {{ agent_role("<id>") }} — a specific role by id
void registerAgentRole(inja::Environment &env, const Bindings &b)
{
    const QString runtimeRole = b.roleId.isEmpty() ? QStringLiteral("developer") : b.roleId;
    env.add_callback("agent_role", 0, [runtimeRole](inja::Arguments &) -> nlohmann::json {
        return roleSystemPrompt(runtimeRole);
    });
    env.add_callback("agent_role", 1, [](inja::Arguments &args) -> nlohmann::json {
        return roleSystemPrompt(QString::fromStdString(args.at(0)->get<std::string>()));
    });
}

void registerSandbox(inja::Environment &env)
{
    
    env.set_search_included_templates_in_files(false);
    env.set_include_callback(
        [](const std::filesystem::path &, const std::string &name) -> inja::Template {
            throw inja::FileError(
                "include is disabled in QodeAssist context: '" + name + "'");
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
    registerAgentRole(env, bindings);

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
        const std::string rendered = env.render(tpl, nlohmann::json::object());
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
