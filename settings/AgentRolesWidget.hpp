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

#include <QWidget>

class QListWidget;
class QPushButton;

namespace QodeAssist::Settings {

class AgentRolesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AgentRolesWidget(QWidget *parent = nullptr);

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
