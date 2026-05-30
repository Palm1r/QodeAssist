// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "EnvBlockFormatter.hpp"

#include <languageserverprotocol/lsptypes.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>

namespace QodeAssist::Context::EnvBlockFormatter {

ProjectEnv currentProject()
{
    ProjectEnv env;
    auto *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project)
        return env;

    env.name = project->displayName();
    env.sourceRoot = project->projectDirectory().toUrlishString();
    if (auto *target = project->activeTarget()) {
        if (auto *buildConfig = target->activeBuildConfiguration())
            env.buildDir = buildConfig->buildDirectory().toUrlishString();
    }
    return env;
}

QString formatProject(const ProjectEnv &env)
{
    if (env.name.isEmpty() && env.sourceRoot.isEmpty())
        return QStringLiteral("# No active project in IDE");

    QString out = QStringLiteral("# Active project: %1").arg(env.name);
    out += QStringLiteral(
               "\n# Project source root: %1"
               "\n#   All new source files, headers, QML and CMake edits MUST be "
               "created or modified under this directory. Use absolute paths "
               "rooted here, or project-relative paths.")
               .arg(env.sourceRoot);
    if (!env.buildDir.isEmpty()) {
        out += QStringLiteral(
                   "\n# Build output directory (compiler artifacts only — do NOT "
                   "create or edit source files here): %1")
                   .arg(env.buildDir);
    }
    return out;
}

QString formatFile(const FileEnv &env)
{
    const QString language
        = LanguageServerProtocol::TextDocumentItem::mimeTypeToLanguageId(env.mimeType);

    QString out = QStringLiteral("File information:");
    if (!language.isEmpty())
        out += QStringLiteral("\nLanguage: %1 (MIME: %2)").arg(language, env.mimeType);
    else if (!env.mimeType.isEmpty())
        out += QStringLiteral("\nMIME type: %1").arg(env.mimeType);
    out += QStringLiteral("\nFile path: %1\n").arg(env.filePath);
    return out;
}

} // namespace QodeAssist::Context::EnvBlockFormatter
