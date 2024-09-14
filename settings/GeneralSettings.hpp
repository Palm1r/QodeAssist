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

#include "providers/LLMProvider.hpp"
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

    Utils::SelectionAspect chatLlmProviders{this};
    Utils::StringAspect chatUrl{this};
    Utils::StringAspect chatEndPoint{this};

    Utils::StringAspect chatModelName{this};
    ButtonAspect chatSelectModels{this};
    Utils::SelectionAspect chatPrompts{this};

    Utils::StringAspect fimModelIndicator{this};
    Utils::StringAspect fimUrlIndicator{this};
    Utils::StringAspect chatModelIndicator{this};
    Utils::StringAspect chatUrlIndicator{this};

private:
    void setupConnections();
    void showModelSelectionDialog(Utils::StringAspect *modelNameObj,
                                  Providers::LLMProvider *provider);
    void resetPageToDefaults();

    void updateStatusIndicators();
    void setIndicatorStatus(Utils::StringAspect &indicator, const QString &tooltip, bool isValid);

    void setCurrentFimProvider(const QString &name);
    void setCurrentChatProvider(const QString &name);

    void loadProviders();
    void loadPrompts();
};

GeneralSettings &generalSettings();

} // namespace QodeAssist::Settings
