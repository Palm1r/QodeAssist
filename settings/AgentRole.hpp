// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
