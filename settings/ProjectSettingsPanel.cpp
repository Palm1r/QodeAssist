// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProjectSettingsPanel.hpp"

#include <projectexplorer/project.h>
#include <projectexplorer/projectpanelfactory.h>
#include <projectexplorer/projectsettingswidget.h>
#include <utils/layoutbuilder.h>

#include <QLabel>
#include <QListWidget>
#include <QPushButton>

#include "ProjectSettings.hpp"
#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SkillsSettings.hpp"
#include "sources/skills/SkillsLoader.hpp"
#include "sources/skills/SkillsManager.hpp"

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

    auto generalWidget = new QWidget;
    Column{
        settings->enableQodeAssist,
        Space{8},
        settings->chatHistoryPath,
    }
        .attachTo(generalWidget);

    generalWidget->setEnabled(!settings->useGlobalSettings());
    QObject::connect(
        widget,
        &ProjectSettingsWidget::useGlobalSettingsChanged,
        generalWidget,
        [generalWidget](bool useGlobal) { generalWidget->setEnabled(!useGlobal); });

    auto skillsList = new QListWidget;
    skillsList->setSelectionMode(QAbstractItemView::NoSelection);
    skillsList->setMaximumHeight(160);

    auto refreshSkills = [skillsList, project, settings] {
        skillsList->clear();

        // Project-only roots: the global page shows global skills separately.
        const QStringList roots = Skills::SkillsManager::resolveRoots(
            project->projectDirectory().toFSPathString(),
            {},
            SkillsSettings::splitLines(settings->projectSkillDirs()));

        const QVector<Skills::AgentSkill> skills = Skills::SkillsLoader::scan(roots);
        for (const Skills::AgentSkill &skill : skills) {
            auto *item = new QListWidgetItem(
                QStringLiteral("%1  —  %2").arg(skill.name, skill.description), skillsList);
            item->setToolTip(skill.skillDir);
        }
        if (skills.isEmpty())
            new QListWidgetItem(Tr::tr("No skills discovered."), skillsList);
    };
    refreshSkills();
    QObject::connect(
        &settings->projectSkillDirs, &Utils::BaseAspect::changed, skillsList, refreshSkills);

    auto *reloadSkillsButton = new QPushButton(Tr::tr("Reload"));
    QObject::connect(reloadSkillsButton, &QPushButton::clicked, skillsList, refreshSkills);

    Column{
        generalWidget,
        Space{8},
        Group{
            title(Tr::tr("Skills")),
            Column{
                settings->projectSkillDirs,
                Row{new QLabel(Tr::tr("Discovered project skills:")), st, reloadSkillsButton},
                skillsList,
            },
        },
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
