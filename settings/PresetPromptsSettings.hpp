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

class PresetPromptsSettings : public Utils::AspectContainer
{
public:
    PresetPromptsSettings();

    Utils::DoubleAspect temperature{this};
    Utils::IntegerAspect maxTokens{this};

    Utils::BoolAspect useTopP{this};
    Utils::DoubleAspect topP{this};

    Utils::BoolAspect useTopK{this};
    Utils::IntegerAspect topK{this};

    Utils::BoolAspect usePresencePenalty{this};
    Utils::DoubleAspect presencePenalty{this};

    Utils::BoolAspect useFrequencyPenalty{this};
    Utils::DoubleAspect frequencyPenalty{this};

    Utils::StringAspect ollamaLivetime{this};
    Utils::StringAspect apiKey{this};

    ButtonAspect resetToDefaults{this};

private:
    void setupConnections();
    void resetSettingsToDefaults();
};

PresetPromptsSettings &presetPromptsSettings();

} // namespace QodeAssist::Settings
