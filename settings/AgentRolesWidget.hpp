// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <coreplugin/dialogs/ioptionspage.h>

class QListWidget;
class QPushButton;

namespace QodeAssist::Settings {

class AgentRolesWidget : public Core::IOptionsPageWidget
{
    Q_OBJECT

public:
    explicit AgentRolesWidget()
    {
        setupUI();
        loadRoles();
    }

private:
    void setupUI();
    void loadRoles();
    void updateButtons();

    void onAddRole();
    void onEditRole();
    void onDuplicateRole();
    void onDeleteRole();
    void onOpenRolesFolder();

    QListWidget *m_rolesList = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_duplicateButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
};

} // namespace QodeAssist::Settings
