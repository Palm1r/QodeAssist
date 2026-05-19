// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SkillsManager.hpp"

#include <QDir>
#include <QFileSystemWatcher>

#include "SkillsLoader.hpp"

namespace QodeAssist::Skills {

SkillsManager::SkillsManager(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
{
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { reload(); });
}

void SkillsManager::configure(
    const QString &projectPath,
    const QStringList &globalRoots,
    const QStringList &projectSubdirs)
{
    if (m_projectPath == projectPath && m_globalRoots == globalRoots
        && m_projectSubdirs == projectSubdirs) {
        return;
    }
    m_projectPath = projectPath;
    m_globalRoots = globalRoots;
    m_projectSubdirs = projectSubdirs;
    reload();
}

QStringList SkillsManager::resolveRoots(
    const QString &projectPath,
    const QStringList &globalRoots,
    const QStringList &projectSubdirs)
{
    // Project-relative roots first so they win on a name collision.
    QStringList roots;
    if (!projectPath.isEmpty()) {
        const QDir projectDir(projectPath);
        const QString projectRoot = QDir::cleanPath(projectDir.absolutePath());
        for (const QString &subdir : projectSubdirs) {
            const QString resolved = QDir::cleanPath(projectDir.absoluteFilePath(subdir));
            // Drop subdirs that escape the project root (e.g. "../../etc").
            if (resolved == projectRoot
                || resolved.startsWith(projectRoot + QLatin1Char('/'))) {
                roots << resolved;
            }
        }
    }
    for (const QString &root : globalRoots)
        roots << QDir::cleanPath(root);
    return roots;
}

void SkillsManager::reload()
{
    const QStringList roots = resolveRoots(m_projectPath, m_globalRoots, m_projectSubdirs);
    m_skills = SkillsLoader::scan(roots);
    updateWatcher(roots);
    emit skillsChanged();
}

void SkillsManager::updateWatcher(const QStringList &roots)
{
    const QStringList watched = m_watcher->directories();
    if (!watched.isEmpty())
        m_watcher->removePaths(watched);

    QStringList toWatch;
    for (const QString &root : roots) {
        if (QDir(root).exists())
            toWatch << root;
    }
    for (const AgentSkill &skill : m_skills)
        toWatch << skill.skillDir;

    if (!toWatch.isEmpty())
        m_watcher->addPaths(toWatch);
}

QVector<AgentSkill> SkillsManager::skills() const
{
    return m_skills;
}

std::optional<AgentSkill> SkillsManager::findByName(const QString &name) const
{
    for (const AgentSkill &skill : m_skills) {
        if (skill.name == name)
            return skill;
    }
    return std::nullopt;
}

QString SkillsManager::catalogText() const
{
    QStringList entries;
    for (const AgentSkill &skill : m_skills) {
        if (!skill.enabled || skill.alwaysOn)
            continue;
        entries << QStringLiteral("- %1: %2").arg(skill.name, skill.description);
    }
    if (entries.isEmpty())
        return {};

    return QStringLiteral("# Available Skills\n"
                          "Specialized skills are available for the tasks below. When a "
                          "request matches a skill, call the load_skill tool with that "
                          "skill's name to load its full instructions, then follow them.\n\n")
           + entries.join('\n');
}

QString SkillsManager::alwaysOnBodies() const
{
    QStringList bodies;
    for (const AgentSkill &skill : m_skills) {
        if (!skill.enabled || !skill.alwaysOn)
            continue;
        if (!skill.body.isEmpty())
            bodies << skill.body;
    }
    return bodies.join(QStringLiteral("\n\n"));
}

} // namespace QodeAssist::Skills
