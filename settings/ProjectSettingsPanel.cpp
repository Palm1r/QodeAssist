// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
