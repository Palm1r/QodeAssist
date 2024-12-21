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

#include "ProjectSettings.hpp"

#include "GeneralSettings.hpp"
#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include <projectexplorer/project.h>

namespace QodeAssist::Settings {

ProjectSettings::ProjectSettings(ProjectExplorer::Project *project)
{
    setAutoApply(true);

    useGlobalSettings.setSettingsKey(Constants::QODE_ASSIST_USE_GLOBAL_SETTINGS);
    useGlobalSettings.setDefaultValue(true);

    enableQodeAssist.setSettingsKey(Constants::QODE_ASSIST_ENABLE_IN_PROJECT);
    enableQodeAssist.setDisplayName(Tr::tr("Enable Qode Assist"));
    enableQodeAssist.setLabelText(Tr::tr("Enable Qode Assist"));
    enableQodeAssist.setDefaultValue(false);

    Utils::Store map = Utils::storeFromVariant(
        project->namedSettings(Constants::QODE_ASSIST_PROJECT_SETTINGS_ID));
    fromMap(map);

    enableQodeAssist.addOnChanged(this, [this, project] { save(project); });
    useGlobalSettings.addOnChanged(this, [this, project] { save(project); });
}

void ProjectSettings::setUseGlobalSettings(bool useGlobal)
{
    useGlobalSettings.setValue(useGlobal);
}

bool ProjectSettings::isEnabled() const
{
    if (useGlobalSettings())
        return generalSettings().enableQodeAssist();
    return enableQodeAssist();
}

void ProjectSettings::save(ProjectExplorer::Project *project)
{
    Utils::Store map;
    toMap(map);
    project
        ->setNamedSettings(Constants::QODE_ASSIST_PROJECT_SETTINGS_ID, Utils::variantFromStore(map));

    // This triggers a restart
    generalSettings().apply();
}

} // namespace QodeAssist::Settings
