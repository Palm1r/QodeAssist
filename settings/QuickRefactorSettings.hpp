// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <utils/aspects.h>

#include "ButtonAspect.hpp"

namespace QodeAssist::Settings {

class QuickRefactorSettings : public Utils::AspectContainer
{
public:
    QuickRefactorSettings();

    ButtonAspect resetToDefaults{this};

    // Context Settings
    Utils::BoolAspect readFullFile{this};
    Utils::BoolAspect readFileParts{this};
    Utils::IntegerAspect readStringsBeforeCursor{this};
    Utils::IntegerAspect readStringsAfterCursor{this};

    // Display Settings
    Utils::SelectionAspect displayMode{this};
    Utils::SelectionAspect widgetOrientation{this};
    Utils::IntegerAspect widgetMinWidth{this};
    Utils::IntegerAspect widgetMaxWidth{this};
    Utils::IntegerAspect widgetMinHeight{this};
    Utils::IntegerAspect widgetMaxHeight{this};

    // Prompt Settings
    Utils::BoolAspect useOpenFilesInQuickRefactor{this};

private:
    void setupConnections();
    void resetSettingsToDefaults();
};

QuickRefactorSettings &quickRefactorSettings();

} // namespace QodeAssist::Settings

