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

#include "settings/SettingsUtils.hpp"

namespace QodeAssist::Settings {

class GeneralSettings : public Utils::AspectContainer
{
public:
    GeneralSettings();

    Utils::BoolAspect enableQodeAssist{this};
    Utils::BoolAspect enableAutoComplete{this};
    Utils::BoolAspect multiLineCompletion{this};
    Utils::BoolAspect enableLogging{this};
    Utils::IntegerAspect startSuggestionTimer{this};
    Utils::IntegerAspect autoCompletionCharThreshold{this};
    Utils::IntegerAspect autoCompletionTypingInterval{this};

    Utils::SelectionAspect llmProviders{this};
    Utils::StringAspect url{this};
    Utils::StringAspect endPoint{this};

    Utils::StringAspect modelName{this};
    ButtonAspect selectModels{this};
    Utils::SelectionAspect fimPrompts{this};
    ButtonAspect resetToDefaults{this};

private:
    void setupConnections();
    void updateProviderSettings();
    void showModelSelectionDialog();
    void resetPageToDefaults();
};

GeneralSettings &generalSettings();

} // namespace QodeAssist::Settings
