// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace QodeAssist::Skills {

struct AgentSkill
{
    QString name;
    QString description;
    QString body;      // Markdown body after the frontmatter
    QString skillDir;  // absolute path to the skill folder
    QString rootPath;  // the scan root this skill was found in
    QString license;
    QString compatibility;
    QStringList allowedTools;
    QHash<QString, QString> metadata;
    bool enabled = true;
    bool alwaysOn = false;

    bool isValid() const { return !name.isEmpty(); }
};

} // namespace QodeAssist::Skills
