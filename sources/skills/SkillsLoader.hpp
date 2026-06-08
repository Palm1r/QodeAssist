// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "AgentSkill.hpp"

namespace QodeAssist::Skills {

class SkillsLoader
{
public:
    struct ParseResult
    {
        AgentSkill skill;
        bool valid = false;
        QString error;
    };

    static QVector<AgentSkill> scan(const QStringList &rootPaths);

    static ParseResult parseSkillFile(const QString &skillDir, const QString &rootPath);

    static int maxBodyChars();

private:
    static bool parseFrontmatter(
        const QString &text, AgentSkill &skill, QString &body, QString &error);
    static bool validateName(const QString &name, const QString &dirName, QString &error);
};

} // namespace QodeAssist::Skills
