// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utils/aspects.h>

namespace QodeAssist::Settings {

class SkillsSettings : public Utils::AspectContainer
{
public:
    SkillsSettings();

    Utils::BoolAspect enableSkills{this};

    Utils::StringAspect globalSkillRoots{this};

    static QStringList splitLines(const QString &value);
    static QStringList splitPaths(const QString &value);
};

SkillsSettings &skillsSettings();

} // namespace QodeAssist::Settings
