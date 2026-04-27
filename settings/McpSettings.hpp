// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utils/aspects.h>

#include "ButtonAspect.hpp"
#include "McpClientsListAspect.hpp"

namespace QodeAssist::Settings {

class McpSettings : public Utils::AspectContainer
{
public:
    McpSettings();

    ButtonAspect resetToDefaults{this};

    Utils::BoolAspect enableMcpServer{this};
    Utils::IntegerAspect mcpServerPort{this};

    ButtonAspect showConnectionInstructions{this};

    Utils::BoolAspect enableMcpClients{this};
    Utils::StringAspect mcpClientExtraPaths{this};
    McpClientsListAspect mcpClientsList{this};

private:
    void setupConnections();
    void resetSettingsToDefaults();
    void showConnectionInstructionsDialog();
};

McpSettings &mcpSettings();

} // namespace QodeAssist::Settings
