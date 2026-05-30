// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <utils/aspects.h>

#include "ButtonAspect.hpp"

namespace QodeAssist::Settings {

class ChatAssistantSettings : public Utils::AspectContainer
{
public:
    ChatAssistantSettings();

    ButtonAspect resetToDefaults{this};

    // Chat settings
    Utils::BoolAspect linkOpenFiles{this};
    Utils::BoolAspect autosave{this};
    Utils::BoolAspect enableChatInBottomToolBar{this};
    Utils::BoolAspect enableChatInNavigationPanel{this};
    Utils::BoolAspect autoCompress{this};
    Utils::IntegerAspect autoCompressThreshold{this};

    // Visuals settings
    Utils::SelectionAspect textFontFamily{this};
    Utils::IntegerAspect textFontSize{this};
    Utils::SelectionAspect codeFontFamily{this};
    Utils::IntegerAspect codeFontSize{this};
    Utils::SelectionAspect textFormat{this};

    Utils::SelectionAspect chatRenderer{this};

private:
    void setupConnections();
    void resetSettingsToDefaults();
};

ChatAssistantSettings &chatAssistantSettings();

} // namespace QodeAssist::Settings
