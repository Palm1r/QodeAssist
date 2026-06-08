// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <utils/aspects.h>

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::Settings {

class ProjectSettings : public Utils::AspectContainer
{
public:
    explicit ProjectSettings(ProjectExplorer::Project *project);
    void save(ProjectExplorer::Project *project);

    void setUseGlobalSettings(bool useGlobalSettings);
    bool isEnabled() const;

    Utils::BoolAspect enableQodeAssist{this};
    Utils::BoolAspect useGlobalSettings{this};
    Utils::FilePathAspect chatHistoryPath{this};
    Utils::StringAspect projectSkillDirs{this};
};

} // namespace QodeAssist::Settings
