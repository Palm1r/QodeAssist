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

#include "ContextSettings.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <utils/layoutbuilder.h>

#include "QodeAssistConstants.hpp"
#include "QodeAssisttr.h"

namespace QodeAssist::Settings {
ContextSettings &contextSettings()
{
    static ContextSettings settings;
    return settings;
}

ContextSettings::ContextSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Context"));

    setLayouter([this]() {
        using namespace Layouting;
        return Column{Stretch{1}};
    });
}

class ContextSettingsPage : public Core::IOptionsPage
{
public:
    ContextSettingsPage()
    {
        setId(Constants::QODE_ASSIST_CONTEXT_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Context"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &contextSettings(); });
    }
};

const ContextSettingsPage contextSettingsPage;

} // namespace QodeAssist::Settings
