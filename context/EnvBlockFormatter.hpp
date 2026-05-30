// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

namespace QodeAssist::Context::EnvBlockFormatter {

struct ProjectEnv
{
    QString name;
    QString sourceRoot;
    QString buildDir;
};

struct FileEnv
{
    QString filePath;
    QString mimeType;
};

ProjectEnv currentProject();

QString formatProject(const ProjectEnv &env);
QString formatFile(const FileEnv &env);

} // namespace QodeAssist::Context::EnvBlockFormatter
