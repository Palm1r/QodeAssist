// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <memory>

#include "session/TurnContextPorts.hpp"

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::Context {
class ContextManager;
}

namespace QodeAssist::Skills {
class SkillsManager;
}

namespace QodeAssist::Chat {

class ProjectContextQtCreator : public Session::IProjectContextPort
{
public:
    explicit ProjectContextQtCreator(ProjectExplorer::Project *project);

    Session::ProjectInfo projectInfo() const override;
    QString projectRules() const override;

private:
    ProjectExplorer::Project *m_project = nullptr;
};

class SkillsContextQtCreator : public Session::ISkillsContextPort
{
public:
    SkillsContextQtCreator(Skills::SkillsManager *skillsManager, ProjectExplorer::Project *project);

    QString alwaysOnBodies() const override;
    QString catalogText() const override;
    std::optional<Session::InvokedSkill> findSkill(const QString &name) const override;

private:
    Skills::SkillsManager *m_skillsManager = nullptr;
};

class LinkedFilesQtCreator : public Session::ILinkedFilesPort
{
public:
    explicit LinkedFilesQtCreator(Context::ContextManager *contextManager);

    QList<Session::LinkedFile> readFiles(const QList<QString> &paths) const override;
    QList<Session::LinkedFile> resolvePaths(const QList<QString> &paths) const override;

private:
    Context::ContextManager *m_contextManager = nullptr;
};

std::unique_ptr<SkillsContextQtCreator> makeSkillsContext(
    Skills::SkillsManager *skillsManager, ProjectExplorer::Project *project);

} // namespace QodeAssist::Chat
