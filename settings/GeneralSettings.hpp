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

namespace QodeAssist::LLMCore {
class Provider;
}
namespace QodeAssist::Settings {

class GeneralSettings : public Utils::AspectContainer
{
public:
    GeneralSettings();

    Utils::BoolAspect enableQodeAssist{this};
    Utils::BoolAspect enableLogging{this};
    Utils::BoolAspect enableCheckUpdate{this};
    ButtonAspect checkUpdate{this};
    ButtonAspect resetToDefaults{this};

    // code completion setttings
    Utils::StringAspect ccProvider{this};
    ButtonAspect ccSelectProvider{this};

    Utils::StringAspect ccModel{this};
    ButtonAspect ccSelectModel{this};

    Utils::StringAspect ccTemplate{this};
    ButtonAspect ccSelectTemplate{this};

    Utils::StringAspect ccUrl{this};
    ButtonAspect ccSetUrl{this};

    Utils::StringAspect ccStatus{this};
    ButtonAspect ccTest{this};

    // TODO create dynamic presets system
    // preset1 for code completion settings
    Utils::BoolAspect specifyPreset1{this};
    Utils::SelectionAspect preset1Language{this};

    Utils::StringAspect ccPreset1Provider{this};
    ButtonAspect ccPreset1SelectProvider{this};

    Utils::StringAspect ccPreset1Url{this};
    ButtonAspect ccPreset1SetUrl{this};

    Utils::StringAspect ccPreset1Model{this};
    ButtonAspect ccPreset1SelectModel{this};

    Utils::StringAspect ccPreset1Template{this};
    ButtonAspect ccPreset1SelectTemplate{this};

    // chat assistant settings
    Utils::StringAspect caProvider{this};
    ButtonAspect caSelectProvider{this};

    Utils::StringAspect caModel{this};
    ButtonAspect caSelectModel{this};

    Utils::StringAspect caTemplate{this};
    ButtonAspect caSelectTemplate{this};

    Utils::StringAspect caUrl{this};
    ButtonAspect caSetUrl{this};

    Utils::StringAspect caStatus{this};
    ButtonAspect caTest{this};

    void showSelectionDialog(const QStringList &data,
                             Utils::StringAspect &aspect,
                             const QString &title = {},
                             const QString &text = {});

    void showModelsNotFoundDialog(Utils::StringAspect &aspect);

    void showModelsNotSupportedDialog(Utils::StringAspect &aspect);

    void showUrlSelectionDialog(Utils::StringAspect &aspect, const QStringList &predefinedUrls);

    void updatePreset1Visiblity(bool state);

private:
    void setupConnections();
    void resetPageToDefaults();
};

GeneralSettings &generalSettings();

} // namespace QodeAssist::Settings
