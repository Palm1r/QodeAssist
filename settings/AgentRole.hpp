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

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace QodeAssist::Settings {

struct AgentRole
{
    QString id;
    QString name;
    QString description;
    QString systemPrompt;
    bool isBuiltin = false;

    QJsonObject toJson() const
    {
        return QJsonObject{
            {"id", id},
            {"name", name},
            {"description", description},
            {"systemPrompt", systemPrompt},
            {"isBuiltin", isBuiltin}};
    }

    static AgentRole fromJson(const QJsonObject &json)
    {
        return AgentRole{
            json["id"].toString(),
            json["name"].toString(),
            json["description"].toString(),
            json["systemPrompt"].toString(),
            json["isBuiltin"].toBool(false)};
    }

    bool operator==(const AgentRole &other) const { return id == other.id; }
};

class AgentRolesManager
{
public:
    static QString getConfigurationDirectory();
    static QList<AgentRole> loadAllRoles();
    static AgentRole loadRole(const QString &roleId);
    static AgentRole loadRoleFromFile(const QString &filePath);
    static bool saveRole(const AgentRole &role);
    static bool deleteRole(const QString &roleId);
    static bool roleExists(const QString &roleId);
    static void ensureDefaultRoles();

    static AgentRole getNoRole()
    {
        return AgentRole{"", "No Role", "Use base system prompt without role specialization", "", false};
    }

private:
    static AgentRole getDefaultDeveloperRole();
    static AgentRole getDefaultReviewerRole();
    static AgentRole getDefaultResearcherRole();
};

} // namespace QodeAssist::Settings
