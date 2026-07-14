// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentRoleController.hpp"

#include <utils/aspects.h>

#include "AgentRole.hpp"
#include "ChatAssistantSettings.hpp"
#include "GeneralSettings.hpp"

namespace QodeAssist::Chat {

AgentRoleController::AgentRoleController(QObject *parent)
    : QObject(parent)
{
    connect(
        &Settings::chatAssistantSettings().systemPrompt,
        &Utils::BaseAspect::changed,
        this,
        &AgentRoleController::baseSystemPromptChanged);

    loadAvailableRoles();
}

QStringList AgentRoleController::availableRoles() const
{
    return m_availableRoles;
}

QString AgentRoleController::currentRole() const
{
    return m_currentRole;
}

QString AgentRoleController::baseSystemPrompt() const
{
    return Settings::chatAssistantSettings().systemPrompt();
}

QString AgentRoleController::currentRoleDescription() const
{
    const QString lastRoleId = Settings::chatAssistantSettings().lastUsedRoleId();
    if (lastRoleId.isEmpty())
        return Settings::AgentRolesManager::getNoRole().description;

    const Settings::AgentRole role = Settings::AgentRolesManager::loadRole(lastRoleId);
    if (role.id.isEmpty())
        return Settings::AgentRolesManager::getNoRole().description;

    return role.description;
}

QString AgentRoleController::currentRoleSystemPrompt() const
{
    const QString lastRoleId = Settings::chatAssistantSettings().lastUsedRoleId();
    if (lastRoleId.isEmpty())
        return QString();

    const Settings::AgentRole role = Settings::AgentRolesManager::loadRole(lastRoleId);
    if (role.id.isEmpty())
        return QString();

    return role.systemPrompt;
}

void AgentRoleController::loadAvailableRoles()
{
    const QList<Settings::AgentRole> roles = Settings::AgentRolesManager::loadAllRoles();

    m_availableRoles.clear();
    m_availableRoles.append(Settings::AgentRolesManager::getNoRole().name);

    for (const auto &role : roles)
        m_availableRoles.append(role.name);

    const QString lastRoleId = Settings::chatAssistantSettings().lastUsedRoleId();
    m_currentRole = Settings::AgentRolesManager::getNoRole().name;

    if (!lastRoleId.isEmpty()) {
        for (const auto &role : roles) {
            if (role.id == lastRoleId) {
                m_currentRole = role.name;
                break;
            }
        }
    }

    emit availableRolesChanged();
    emit currentRoleChanged();
}

void AgentRoleController::applyRole(const QString &roleName)
{
    auto &settings = Settings::chatAssistantSettings();

    if (roleName == Settings::AgentRolesManager::getNoRole().name) {
        settings.lastUsedRoleId.setValue("");
        settings.writeSettings();
        m_currentRole = roleName;
        emit currentRoleChanged();
        return;
    }

    const QList<Settings::AgentRole> roles = Settings::AgentRolesManager::loadAllRoles();

    for (const auto &role : roles) {
        if (role.name == roleName) {
            settings.lastUsedRoleId.setValue(role.id);
            settings.writeSettings();
            m_currentRole = role.name;
            emit currentRoleChanged();
            break;
        }
    }
}

void AgentRoleController::openSettings()
{
    Settings::showSettings(Utils::Id("QodeAssist.AgentRoles"));
}

} // namespace QodeAssist::Chat
