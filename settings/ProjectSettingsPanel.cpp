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

#include "ProjectSettingsPanel.hpp"

#include <projectexplorer/project.h>
#include <projectexplorer/projectpanelfactory.h>
#include <projectexplorer/projectsettingswidget.h>
#include <utils/layoutbuilder.h>

#include "ProjectSettings.hpp"
#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"

using namespace ProjectExplorer;

namespace QodeAssist::Settings {

class ProjectSettingsWidget final : public ProjectExplorer::ProjectSettingsWidget
{
public:
    ProjectSettingsWidget()
    {
        setGlobalSettingsId(Constants::QODE_ASSIST_GENERAL_OPTIONS_ID);
        setUseGlobalSettingsCheckBoxVisible(true);
    }
};

static ProjectSettingsWidget *createProjectPanel(Project *project)
{
    using namespace Layouting;

    auto widget = new ProjectSettingsWidget;
    auto settings = new ProjectSettings(project);
    settings->setParent(widget);

    QObject::connect(
        widget,
        &ProjectSettingsWidget::useGlobalSettingsChanged,
        settings,
        &ProjectSettings::setUseGlobalSettings);

    widget->setUseGlobalSettings(settings->useGlobalSettings());
    widget->setEnabled(!settings->useGlobalSettings());

    QObject::connect(
        widget, &ProjectSettingsWidget::useGlobalSettingsChanged, widget, [widget](bool useGlobal) {
            widget->setEnabled(!useGlobal);
        });

    Column{
        settings->enableQodeAssist,
        Space{8},
        settings->chatHistoryPath,
    }
        .attachTo(widget);

    return widget;
}

class ProjectPanelFactory final : public ProjectExplorer::ProjectPanelFactory
{
public:
    ProjectPanelFactory()
    {
        setPriority(1000);
        setDisplayName(Tr::tr("QodeAssist"));
        setCreateWidgetFunction(&createProjectPanel);
    }
};

void setupProjectPanel()
{
    static ProjectPanelFactory theProjectPanelFactory;
}

} // namespace QodeAssist::Settings
