/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include "settings/SettingsUtils.hpp"
#include <utils/aspects.h>

namespace QodeAssist::Settings {

class CustomPromptSettings : public Utils::AspectContainer
{
public:
    CustomPromptSettings();

    Utils::StringAspect customJsonLabel{this};
    Utils::StringAspect customJsonTemplate{this};
    Utils::StringAspect customJsonLegend{this};
    ButtonAspect saveCustomTemplateButton{this};
    ButtonAspect loadCustomTemplateButton{this};
    ButtonAspect resetToDefaults{this};

private:
    void setupConnection();
    void resetSettingsToDefaults();
    void saveCustomTemplate();
    void loadCustomTemplate();
};

CustomPromptSettings &customPromptSettings();

} // namespace QodeAssist::Settings
