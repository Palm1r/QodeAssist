/* 
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

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
