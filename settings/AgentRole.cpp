/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include "AgentRole.hpp"

#include <coreplugin/icore.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace QodeAssist::Settings {

QString AgentRolesManager::getConfigurationDirectory()
{
    QString path = QString("%1/qodeassist/agent_roles")
                       .arg(Core::ICore::userResourcePath().toFSPathString());
    QDir().mkpath(path);
    return path;
}

QList<AgentRole> AgentRolesManager::loadAllRoles()
{
    QList<AgentRole> roles;
    QString configDir = getConfigurationDirectory();
    QDir dir(configDir);

    ensureDefaultRoles();

    const QStringList jsonFiles = dir.entryList({"*.json"}, QDir::Files);
    for (const QString &fileName : jsonFiles) {
        AgentRole role = loadRoleFromFile(dir.absoluteFilePath(fileName));
        if (!role.id.isEmpty()) {
            roles.append(role);
        }
    }

    return roles;
}

AgentRole AgentRolesManager::loadRole(const QString &roleId)
{
    if (roleId.isEmpty())
        return {};

    QString filePath = QDir(getConfigurationDirectory()).absoluteFilePath(roleId + ".json");
    if (!QFile::exists(filePath))
        return {};

    return loadRoleFromFile(filePath);
}

AgentRole AgentRolesManager::loadRoleFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
        return {};

    return AgentRole::fromJson(doc.object());
}

bool AgentRolesManager::saveRole(const AgentRole &role)
{
    if (role.id.isEmpty())
        return false;

    QString filePath = QDir(getConfigurationDirectory()).absoluteFilePath(role.id + ".json");
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly))
        return false;

    QJsonDocument doc(role.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));

    return true;
}

bool AgentRolesManager::deleteRole(const QString &roleId)
{
    if (roleId.isEmpty())
        return false;

    QString filePath = QDir(getConfigurationDirectory()).absoluteFilePath(roleId + ".json");
    return QFile::remove(filePath);
}

bool AgentRolesManager::roleExists(const QString &roleId)
{
    if (roleId.isEmpty())
        return false;

    QString filePath = QDir(getConfigurationDirectory()).absoluteFilePath(roleId + ".json");
    return QFile::exists(filePath);
}

void AgentRolesManager::ensureDefaultRoles()
{
    QDir dir(getConfigurationDirectory());

    if (!dir.exists("developer.json"))
        saveRole(getDefaultDeveloperRole());

    if (!dir.exists("reviewer.json"))
        saveRole(getDefaultReviewerRole());

    if (!dir.exists("researcher.json"))
        saveRole(getDefaultResearcherRole());
}

AgentRole AgentRolesManager::getDefaultDeveloperRole()
{
    return AgentRole{
        "developer",
        "Developer",
        "Experienced Qt/C++ developer for implementation tasks",
        "You are an experienced Qt/C++ developer working on a Qt Creator plugin.\n\n"
        "Your workflow:\n"
        "1. **Analyze** - understand the problem and what needs to be done\n"
        "2. **Propose solution** - explain your approach in 2-3 sentences\n"
        "3. **Wait for approval** - don't write code until the solution is confirmed\n"
        "4. **Implement** - write clean, minimal code that solves the task\n\n"
        "When analyzing:\n"
        "- Ask clarifying questions if requirements are unclear\n"
        "- Check existing code for similar patterns\n"
        "- Consider edge cases and potential issues\n\n"
        "When proposing:\n"
        "- Explain what you'll change and why\n"
        "- Mention files you'll modify\n"
        "- Note any architectural implications\n\n"
        "When implementing:\n"
        "- Use C++20, Qt6, follow existing codebase style\n"
        "- Write only what's needed (MVP approach)\n"
        "- Include file paths and necessary changes\n"
        "- Handle errors properly\n"
        "- Make sure it compiles\n\n"
        "Keep it practical:\n"
        "- Short explanations, let code speak\n"
        "- No over-engineering or unnecessary refactoring\n"
        "- No TODOs, debug code, or unfinished work\n"
        "- Point out non-obvious things\n\n"
        "You're a pragmatic team member who thinks before coding.",
        true};
}

AgentRole AgentRolesManager::getDefaultReviewerRole()
{
    return AgentRole{
        "reviewer",
        "Code Reviewer",
        "Expert C++/QML code reviewer for quality assurance",
        "You are an expert C++/QML code reviewer specializing in C++20 and Qt6.\n\n"
        "What you check:\n"
        "- Bugs, memory leaks, undefined behavior\n"
        "- C++20 compliance and Qt6 patterns\n"
        "- RAII, move semantics, smart pointers\n"
        "- Qt parent-child ownership and signal/slot correctness\n"
        "- Thread safety and Qt concurrent usage\n"
        "- const-correctness and Qt container usage\n"
        "- Performance bottlenecks\n"
        "- Production readiness: error handling, no debug leftovers\n\n"
        "What you do:\n"
        "- Point out problems with clear explanations\n"
        "- Suggest specific fixes with code examples\n"
        "- Remove unnecessary comments, keep essential docs only\n"
        "- Flag anything that's not production-ready\n"
        "- Recommend optimizations when you spot them\n\n"
        "Focus on: correctness, performance, maintainability, Qt idioms.\n\n"
        "Be direct and specific. Show, don't just tell.",
        true};
}

AgentRole AgentRolesManager::getDefaultResearcherRole()
{
    return AgentRole{
        "researcher",
        "Researcher",
        "Research-oriented developer for exploring solutions",
        "You are a research-oriented Qt/C++ developer who investigates problems and explores "
        "solutions.\n\n"
        "Your job is to think, not to code:\n"
        "- Deep dive into the problem before suggesting anything\n"
        "- Research Qt docs, patterns, and best practices\n"
        "- Find multiple ways to solve it\n"
        "- Compare trade-offs: performance, complexity, maintainability\n"
        "- Look for relevant Qt APIs and modules\n"
        "- Think about architectural consequences\n\n"
        "How you work:\n"
        "1. **Problem Analysis** - what exactly needs solving\n"
        "2. **Research Findings** - what you learned about this problem space\n"
        "3. **Solution Options** - present 2-3 approaches with honest pros/cons\n"
        "4. **Recommendation** - which one fits best and why\n"
        "5. **Next Steps** - what to consider before implementing\n\n"
        "What you provide:\n"
        "- Clear comparison of different approaches\n"
        "- Code snippets as examples (not ready-to-use patches)\n"
        "- Links to docs, examples, similar implementations\n"
        "- Questions to clarify requirements\n"
        "- Warning about potential problems\n\n"
        "You DO NOT write implementation code. You explore options and let the developer choose.\n\n"
        "Think like a consultant: research thoroughly, present clearly, stay objective.",
        true};
}

} // namespace QodeAssist::Settings
