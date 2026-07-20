// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "TurnContextAdapters.hpp"

#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>

#include "ProjectSettings.hpp"
#include "SkillsSettings.hpp"
#include "skills/SkillsManager.hpp"

namespace QodeAssist::Chat {

ProjectExplorer::Project *activeProject()
{
    auto currentEditor = Core::EditorManager::currentEditor();
    if (currentEditor && currentEditor->document()) {
        auto project = ProjectExplorer::ProjectManager::projectForFile(
            currentEditor->document()->filePath());
        if (project)
            return project;
    }

    return ProjectExplorer::ProjectManager::startupProject();
}

ProjectContextQtCreator::ProjectContextQtCreator(ProjectExplorer::Project *project)
    : m_project(project)
{}

Session::ProjectInfo ProjectContextQtCreator::projectInfo() const
{
    if (!m_project)
        return {};

    Session::ProjectInfo info;
    info.available = true;
    info.displayName = m_project->displayName();
    info.sourceRoot = m_project->projectDirectory().toUrlishString();

    if (auto target = m_project->activeTarget()) {
        if (auto buildConfig = target->activeBuildConfiguration())
            info.buildDirectory = buildConfig->buildDirectory().toUrlishString();
    }

    return info;
}

SkillsContextQtCreator::SkillsContextQtCreator(
    Skills::SkillsManager *skillsManager, ProjectExplorer::Project *project)
    : m_skillsManager(skillsManager)
{
    QStringList projectSkillDirs;
    if (project) {
        Settings::ProjectSettings projectSettings(project);
        projectSkillDirs = Settings::SkillsSettings::splitLines(projectSettings.projectSkillDirs());
    }

    m_skillsManager->configure(
        project ? project->projectDirectory().toFSPathString() : QString(),
        Settings::SkillsSettings::splitPaths(Settings::skillsSettings().globalSkillRoots()),
        projectSkillDirs);
}

QString SkillsContextQtCreator::alwaysOnBodies() const
{
    return m_skillsManager->alwaysOnBodies();
}

QString SkillsContextQtCreator::catalogText() const
{
    return m_skillsManager->catalogText();
}

std::optional<Session::InvokedSkill> SkillsContextQtCreator::findSkill(const QString &name) const
{
    const auto skill = m_skillsManager->findByName(name);
    if (!skill)
        return std::nullopt;

    return Session::InvokedSkill{skill->name, skill->body};
}

std::unique_ptr<SkillsContextQtCreator> makeSkillsContext(
    Skills::SkillsManager *skillsManager, ProjectExplorer::Project *project)
{
    if (!skillsManager || !Settings::skillsSettings().enableSkills())
        return nullptr;

    return std::make_unique<SkillsContextQtCreator>(skillsManager, project);
}

} // namespace QodeAssist::Chat
