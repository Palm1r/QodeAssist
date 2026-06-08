// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QStringList>

namespace QodeAssist::Chat {

class AgentRoleController : public QObject
{
    Q_OBJECT

public:
    explicit AgentRoleController(QObject *parent = nullptr);

    QStringList availableRoles() const;
    QString currentRole() const;
    QString baseSystemPrompt() const;
    QString currentRoleDescription() const;
    QString currentRoleSystemPrompt() const;

    void loadAvailableRoles();
    void applyRole(const QString &roleName);
    void openSettings();

signals:
    void availableRolesChanged();
    void currentRoleChanged();
    void baseSystemPromptChanged();

private:
    QStringList m_availableRoles;
    QString m_currentRole;
};

} // namespace QodeAssist::Chat
