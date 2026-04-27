// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProjectSettings.hpp"

#include "GeneralSettings.hpp"
#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include <coreplugin/icore.h>
#include <projectexplorer/project.h>

namespace QodeAssist::Settings {

ProjectSettings::ProjectSettings(ProjectExplorer::Project *project)
{
    setAutoApply(true);

    useGlobalSettings.setSettingsKey(Constants::QODE_ASSIST_USE_GLOBAL_SETTINGS);
    useGlobalSettings.setDefaultValue(true);

    enableQodeAssist.setSettingsKey(Constants::QODE_ASSIST_ENABLE_IN_PROJECT);
    enableQodeAssist.setDisplayName(Tr::tr("Enable QodeAssist"));
    enableQodeAssist.setLabelText(Tr::tr("Enable QodeAssist"));
    enableQodeAssist.setDefaultValue(false);

    chatHistoryPath.setSettingsKey(Constants::QODE_ASSIST_CHAT_HISTORY_PATH);
    chatHistoryPath.setExpectedKind(Utils::PathChooser::ExistingDirectory);
    chatHistoryPath.setLabelText(Tr::tr("Chat History Path:"));

    QString projectChatHistoryPath = QString("%1/qodeassist/chat_history")
                                         .arg(Core::ICore::userResourcePath().toFSPathString());

    chatHistoryPath.setDefaultValue(projectChatHistoryPath);

    Utils::Store map = Utils::storeFromVariant(
        project->namedSettings(Constants::QODE_ASSIST_PROJECT_SETTINGS_ID));
    fromMap(map);

    enableQodeAssist.addOnChanged(this, [this, project] { save(project); });
    useGlobalSettings.addOnChanged(this, [this, project] { save(project); });
    chatHistoryPath.addOnChanged(this, [this, project] { save(project); });
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
    generalSettings().apply();
}

} // namespace QodeAssist::Settings
