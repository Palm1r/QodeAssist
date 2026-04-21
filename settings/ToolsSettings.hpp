// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utils/aspects.h>

#include "ButtonAspect.hpp"

namespace QodeAssist::Settings {

class ToolsSettings : public Utils::AspectContainer
{
public:
    ToolsSettings();

    ButtonAspect resetToDefaults{this};

    Utils::BoolAspect allowAccessOutsideProject{this};
    Utils::BoolAspect autoApplyFileEdits{this};

    Utils::BoolAspect enableListProjectFilesTool{this};
    Utils::BoolAspect enableFindFileTool{this};
    Utils::BoolAspect enableReadFileTool{this};
    Utils::BoolAspect enableProjectSearchTool{this};
    Utils::BoolAspect enableCreateNewFileTool{this};
    Utils::BoolAspect enableEditFileTool{this};
    Utils::BoolAspect enableBuildProjectTool{this};
    Utils::BoolAspect enableGetIssuesListTool{this};
    Utils::BoolAspect enableTerminalCommandTool{this};
    Utils::BoolAspect enableTodoTool{this};

    Utils::StringAspect allowedTerminalCommandsLinux{this};
    Utils::StringAspect allowedTerminalCommandsMacOS{this};
    Utils::StringAspect allowedTerminalCommandsWindows{this};
    Utils::IntegerAspect terminalCommandTimeout{this};

private:
    void setupConnections();
    void resetSettingsToDefaults();
};

ToolsSettings &toolsSettings();

} // namespace QodeAssist::Settings
