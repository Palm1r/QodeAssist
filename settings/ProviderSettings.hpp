// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utils/aspects.h>

#include "ButtonAspect.hpp"

namespace QodeAssist::Settings {

class ProviderSettings : public Utils::AspectContainer
{
public:
    ProviderSettings();

    ButtonAspect resetToDefaults{this};

    // API Keys
    Utils::StringAspect openRouterApiKey{this};
    Utils::StringAspect openAiCompatApiKey{this};
    Utils::StringAspect claudeApiKey{this};
    Utils::StringAspect openAiApiKey{this};
    Utils::StringAspect mistralAiApiKey{this};
    Utils::StringAspect codestralApiKey{this};
    Utils::StringAspect googleAiApiKey{this};
    Utils::StringAspect ollamaBasicAuthApiKey{this};

private:
    void setupConnections();
    void resetSettingsToDefaults();
};

ProviderSettings &providerSettings();

} // namespace QodeAssist::Settings
