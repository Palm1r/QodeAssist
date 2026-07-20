// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "TurnContextAdapters.hpp"

#include <QFileInfo>

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/target.h>

#include "ProjectSettings.hpp"
#include "SkillsSettings.hpp"
#include "context/ContextManager.hpp"
#include "context/RulesLoader.hpp"
#include "skills/SkillsManager.hpp"

namespace QodeAssist::Chat {

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

QString ProjectContextQtCreator::projectRules() const
{
    if (!m_project)
        return {};

    return Context::RulesLoader::loadRulesForProject(m_project, Context::RulesContext::Chat);
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

LinkedFilesQtCreator::LinkedFilesQtCreator(Context::ContextManager *contextManager)
    : m_contextManager(contextManager)
{}

QList<Session::LinkedFile> LinkedFilesQtCreator::readFiles(const QList<QString> &paths) const
{
    QList<Session::LinkedFile> files;
    for (const auto &file : m_contextManager->getContentFiles(paths))
        files.append(Session::LinkedFile{file.filename, file.content, file.path});

    return files;
}

QList<Session::LinkedFile> LinkedFilesQtCreator::resolvePaths(const QList<QString> &paths) const
{
    QList<Session::LinkedFile> files;
    for (const QString &path : m_contextManager->allowedPaths(paths)) {
        const QFileInfo fileInfo(path);
        files.append(Session::LinkedFile{fileInfo.fileName(), {}, fileInfo.absoluteFilePath()});
    }

    return files;
}

std::unique_ptr<SkillsContextQtCreator> makeSkillsContext(
    Skills::SkillsManager *skillsManager, ProjectExplorer::Project *project)
{
    if (!skillsManager || !Settings::skillsSettings().enableSkills())
        return nullptr;

    return std::make_unique<SkillsContextQtCreator>(skillsManager, project);
}

} // namespace QodeAssist::Chat
