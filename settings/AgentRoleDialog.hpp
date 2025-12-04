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

#include <QDialog>

#include "AgentRole.hpp"

class QLineEdit;
class QTextEdit;
class QDialogButtonBox;

namespace QodeAssist::Settings {

class AgentRoleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AgentRoleDialog(QWidget *parent = nullptr);
    explicit AgentRoleDialog(const AgentRole &role, bool editMode = true, QWidget *parent = nullptr);

    AgentRole getRole() const;
    void setRole(const AgentRole &role);

private:
    void setupUI();
    void validateInput();

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_idEdit = nullptr;
    QTextEdit *m_descriptionEdit = nullptr;
    QTextEdit *m_systemPromptEdit = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    bool m_editMode = false;
};

} // namespace QodeAssist::Settings
