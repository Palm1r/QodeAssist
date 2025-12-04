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
}

AgentRole AgentRolesManager::getDefaultDeveloperRole()
{
    return AgentRole{
        "developer",
        "Developer",
        "General coding assistance and implementation",
        "You are an advanced AI assistant specializing in C++, Qt, and QML development. "
        "Your role is to provide helpful, accurate, and detailed responses to questions "
        "about coding, debugging, and best practices in these technologies. "
        "Focus on writing clean, maintainable code following industry standards.",
        false};
}

AgentRole AgentRolesManager::getDefaultReviewerRole()
{
    return AgentRole{
        "reviewer",
        "Code Reviewer",
        "Code review, quality assurance, and best practices",
        "You are an expert code reviewer specializing in C++, Qt, and QML. "
        "Your role is to:\n"
        "- Identify potential bugs, memory leaks, and performance issues\n"
        "- Check adherence to coding standards and best practices\n"
        "- Suggest improvements for readability and maintainability\n"
        "- Verify proper error handling and edge cases\n"
        "- Ensure thread safety and proper Qt object lifetime management\n"
        "Provide constructive, specific feedback with examples.",
        false};
}

} // namespace QodeAssist::Settings
