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

class ContextSettings : public Utils::AspectContainer
{
public:
    ContextSettings();

    Utils::BoolAspect readFullFile{this};
    Utils::IntegerAspect readStringsBeforeCursor{this};
    Utils::IntegerAspect readStringsAfterCursor{this};

    Utils::BoolAspect useSystemPrompt{this};
    Utils::StringAspect systemPrompt{this};
    Utils::BoolAspect useFilePathInContext{this};
    Utils::BoolAspect useProjectChangesCache{this};
    Utils::IntegerAspect maxChangesCacheSize{this};
    Utils::BoolAspect useChatSystemPrompt{this};
    Utils::StringAspect chatSystemPrompt{this};

    ButtonAspect resetToDefaults{this};

private:
    void setupConnection();
    void resetPageToDefaults();
};

ContextSettings &contextSettings();

} // namespace QodeAssist::Settings
