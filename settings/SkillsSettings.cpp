// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SkillsSettings.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>

#include <QDir>
#include <QLabel>
#include <QListWidget>

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "sources/skills/SkillsLoader.hpp"

namespace QodeAssist::Settings {

SkillsSettings &skillsSettings()
{
    static SkillsSettings settings;
    return settings;
}

QStringList SkillsSettings::splitLines(const QString &value)
{
    QStringList lines;
    const auto parts = value.split('\n', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
            lines << trimmed;
    }
    return lines;
}

QStringList SkillsSettings::splitPaths(const QString &value)
{
    QStringList paths;
    for (QString path : splitLines(value)) {
        if (path == QLatin1String("~"))
            path = QDir::homePath();
        else if (path.startsWith(QLatin1String("~/")))
            path = QDir::homePath() + path.mid(1);
        paths << QDir::cleanPath(path);
    }
    return paths;
}

SkillsSettings::SkillsSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Skills"));

    enableSkills.setSettingsKey(Constants::SK_ENABLE_SKILLS);
    enableSkills.setLabelText(Tr::tr("Enable skills"));
    enableSkills.setToolTip(
        Tr::tr("Discover Agent Skills from the configured skill directories and expose them "
               "to the chat assistant. Each skill is a folder containing a SKILL.md file."));
    enableSkills.setDefaultValue(true);

    const QString defaultGlobalRoots
        = Core::ICore::userResourcePath().toFSPathString() + "/qodeassist/skills\n"
          + QDir::homePath() + "/.claude/skills";

    globalSkillRoots.setSettingsKey(Constants::SK_GLOBAL_SKILL_ROOTS);
    globalSkillRoots.setLabelText(Tr::tr("Global skill directories:"));
    globalSkillRoots.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    globalSkillRoots.setToolTip(
        Tr::tr("Absolute paths scanned for skills, one per line. Each path is a directory "
               "whose subfolders contain SKILL.md files. A leading ~ expands to your home "
               "directory. Lets QodeAssist pick up skills shared with other agents "
               "(e.g. ~/.claude/skills)."));
    globalSkillRoots.setDefaultValue(defaultGlobalRoots);

    readSettings();

    setLayouter([this]() {
        using namespace Layouting;

        auto skillsList = new QListWidget;
        skillsList->setSelectionMode(QAbstractItemView::NoSelection);
        skillsList->setMaximumHeight(160);

        auto refreshSkills = [skillsList, this] {
            skillsList->clear();
            const QVector<Skills::AgentSkill> skills
                = Skills::SkillsLoader::scan(splitPaths(globalSkillRoots()));
            for (const Skills::AgentSkill &skill : skills) {
                auto *item = new QListWidgetItem(
                    QStringLiteral("%1  —  %2").arg(skill.name, skill.description), skillsList);
                item->setToolTip(skill.skillDir);
            }
            if (skills.isEmpty())
                new QListWidgetItem(Tr::tr("No skills discovered."), skillsList);
        };
        refreshSkills();
        connect(&globalSkillRoots, &Utils::BaseAspect::changed, skillsList, refreshSkills);

        return Column{
            Group{
                title(Tr::tr("Skills")),
                Column{
                    Row{enableSkills, Stretch{1}},
                },
            },
            Group{
                title(Tr::tr("Skill Directories")),
                Column{
                    globalSkillRoots,
                    new QLabel(Tr::tr("Discovered global skills:")),
                    skillsList,
                },
            },
            Stretch{1}};
    });
}

class SkillsSettingsPage : public Core::IOptionsPage
{
public:
    SkillsSettingsPage()
    {
        setId(Constants::QODE_ASSIST_SKILLS_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Skills"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &skillsSettings(); });
    }
};

const SkillsSettingsPage skillsSettingsPage;

} // namespace QodeAssist::Settings
