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

#include <utils/aspects.h>

#include "ButtonAspect.hpp"

namespace QodeAssist::Settings {

class PresetPromptsSettings : public Utils::AspectContainer
{
public:
    struct PromptSettings
    {
        double temperature;
        int maxTokens;
        bool useTopP;
        double topP;
        bool useTopK;
        int topK;
        bool usePresencePenalty;
        double presencePenalty;
        bool useFrequencyPenalty;
        double frequencyPenalty;
        QString ollamaLivetime;
        QString apiKey;
    };

    PresetPromptsSettings();

    Utils::DoubleAspect fimTemperature{this};
    Utils::IntegerAspect fimMaxTokens{this};

    Utils::DoubleAspect chatTemperature{this};
    Utils::IntegerAspect chatMaxTokens{this};

    Utils::BoolAspect fimUseTopP{this};
    Utils::DoubleAspect fimTopP{this};

    Utils::BoolAspect chatUseTopP{this};
    Utils::DoubleAspect chatTopP{this};

    Utils::BoolAspect fimUseTopK{this};
    Utils::IntegerAspect fimTopK{this};

    Utils::BoolAspect chatUseTopK{this};
    Utils::IntegerAspect chatTopK{this};

    Utils::BoolAspect fimUsePresencePenalty{this};
    Utils::DoubleAspect fimPresencePenalty{this};

    Utils::BoolAspect chatUsePresencePenalty{this};
    Utils::DoubleAspect chatPresencePenalty{this};

    Utils::BoolAspect fimUseFrequencyPenalty{this};
    Utils::DoubleAspect fimFrequencyPenalty{this};

    Utils::BoolAspect chatUseFrequencyPenalty{this};
    Utils::DoubleAspect chatFrequencyPenalty{this};

    Utils::StringAspect fimOllamaLivetime{this};
    Utils::StringAspect chatOllamaLivetime{this};
    Utils::StringAspect fimApiKey{this};
    Utils::StringAspect chatApiKey{this};

    ButtonAspect resetToDefaults{this};

    PromptSettings getSettings(int type) const;

private:
    void setupConnections();
    void resetSettingsToDefaults();
};

PresetPromptsSettings &presetPromptsSettings();

} // namespace QodeAssist::Settings
