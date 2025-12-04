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

#include "AgentRolesWidget.hpp"

#include "AgentRole.hpp"
#include "AgentRoleDialog.hpp"
#include "SettingsTr.hpp"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace QodeAssist::Settings {

AgentRolesWidget::AgentRolesWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    loadRoles();
}

void AgentRolesWidget::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *headerLayout = new QHBoxLayout();

    auto *infoLabel = new QLabel(
        Tr::tr("Agent roles define different system prompts for specific tasks."), this);
    infoLabel->setWordWrap(true);
    headerLayout->addWidget(infoLabel, 1);

    auto *openFolderButton = new QPushButton(Tr::tr("Open Roles Folder..."), this);
    connect(openFolderButton, &QPushButton::clicked, this, &AgentRolesWidget::onOpenRolesFolder);
    headerLayout->addWidget(openFolderButton);

    mainLayout->addLayout(headerLayout);

    auto *contentLayout = new QHBoxLayout();

    m_rolesList = new QListWidget(this);
    m_rolesList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_rolesList, &QListWidget::itemSelectionChanged, this, &AgentRolesWidget::updateButtons);
    connect(m_rolesList, &QListWidget::itemDoubleClicked, this, &AgentRolesWidget::onEditRole);
    contentLayout->addWidget(m_rolesList, 1);

    auto *buttonsLayout = new QVBoxLayout();

    m_addButton = new QPushButton(Tr::tr("Add..."), this);
    connect(m_addButton, &QPushButton::clicked, this, &AgentRolesWidget::onAddRole);
    buttonsLayout->addWidget(m_addButton);

    m_editButton = new QPushButton(Tr::tr("Edit..."), this);
    connect(m_editButton, &QPushButton::clicked, this, &AgentRolesWidget::onEditRole);
    buttonsLayout->addWidget(m_editButton);

    m_duplicateButton = new QPushButton(Tr::tr("Duplicate..."), this);
    connect(m_duplicateButton, &QPushButton::clicked, this, &AgentRolesWidget::onDuplicateRole);
    buttonsLayout->addWidget(m_duplicateButton);

    m_deleteButton = new QPushButton(Tr::tr("Delete"), this);
    connect(m_deleteButton, &QPushButton::clicked, this, &AgentRolesWidget::onDeleteRole);
    buttonsLayout->addWidget(m_deleteButton);

    buttonsLayout->addStretch();

    contentLayout->addLayout(buttonsLayout);
    mainLayout->addLayout(contentLayout);

    updateButtons();
}

void AgentRolesWidget::loadRoles()
{
    m_rolesList->clear();

    const QList<AgentRole> roles = AgentRolesManager::loadAllRoles();
    for (const AgentRole &role : roles) {
        auto *item = new QListWidgetItem(role.name, m_rolesList);
        item->setData(Qt::UserRole, role.id);

        QString tooltip = role.description;
        if (role.isBuiltin) {
            tooltip += "\n\n" + Tr::tr("(Built-in role)");
            item->setForeground(Qt::darkGray);
        }
        item->setToolTip(tooltip);
    }
}

void AgentRolesWidget::updateButtons()
{
    QListWidgetItem *selectedItem = m_rolesList->currentItem();
    bool hasSelection = selectedItem != nullptr;
    bool isBuiltin = false;

    if (hasSelection) {
        QString roleId = selectedItem->data(Qt::UserRole).toString();
        AgentRole role = AgentRolesManager::loadRole(roleId);
        isBuiltin = role.isBuiltin;
    }

    m_editButton->setEnabled(hasSelection);
    m_duplicateButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection && !isBuiltin);
}

void AgentRolesWidget::onAddRole()
{
    AgentRoleDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    AgentRole newRole = dialog.getRole();

    if (AgentRolesManager::roleExists(newRole.id)) {
        QMessageBox::warning(
            this,
            Tr::tr("Role Already Exists"),
            Tr::tr("A role with ID '%1' already exists. Please use a different ID.")
                .arg(newRole.id));
        return;
    }

    if (AgentRolesManager::saveRole(newRole)) {
        loadRoles();
    } else {
        QMessageBox::critical(
            this, Tr::tr("Error"), Tr::tr("Failed to save role '%1'.").arg(newRole.name));
    }
}

void AgentRolesWidget::onEditRole()
{
    QListWidgetItem *selectedItem = m_rolesList->currentItem();
    if (!selectedItem)
        return;

    QString roleId = selectedItem->data(Qt::UserRole).toString();
    AgentRole role = AgentRolesManager::loadRole(roleId);

    if (role.isBuiltin) {
        QMessageBox::information(
            this,
            Tr::tr("Cannot Edit Built-in Role"),
            Tr::tr(
                "Built-in roles cannot be edited. You can duplicate this role and modify the copy."));
        return;
    }

    AgentRoleDialog dialog(role, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    AgentRole updatedRole = dialog.getRole();

    if (AgentRolesManager::saveRole(updatedRole)) {
        loadRoles();
    } else {
        QMessageBox::critical(
            this, Tr::tr("Error"), Tr::tr("Failed to update role '%1'.").arg(updatedRole.name));
    }
}

void AgentRolesWidget::onDuplicateRole()
{
    QListWidgetItem *selectedItem = m_rolesList->currentItem();
    if (!selectedItem)
        return;

    QString roleId = selectedItem->data(Qt::UserRole).toString();
    AgentRole role = AgentRolesManager::loadRole(roleId);

    role.name += " (Copy)";
    role.id += "_copy";
    role.isBuiltin = false;

    int counter = 1;
    QString baseId = role.id;
    while (AgentRolesManager::roleExists(role.id)) {
        role.id = baseId + QString::number(counter++);
    }

    AgentRoleDialog dialog(role, false, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    AgentRole newRole = dialog.getRole();

    if (AgentRolesManager::roleExists(newRole.id)) {
        QMessageBox::warning(
            this,
            Tr::tr("Role Already Exists"),
            Tr::tr("A role with ID '%1' already exists. Please use a different ID.")
                .arg(newRole.id));
        return;
    }

    if (AgentRolesManager::saveRole(newRole)) {
        loadRoles();
    } else {
        QMessageBox::critical(this, Tr::tr("Error"), Tr::tr("Failed to duplicate role."));
    }
}

void AgentRolesWidget::onDeleteRole()
{
    QListWidgetItem *selectedItem = m_rolesList->currentItem();
    if (!selectedItem)
        return;

    QString roleId = selectedItem->data(Qt::UserRole).toString();
    AgentRole role = AgentRolesManager::loadRole(roleId);

    if (role.isBuiltin) {
        QMessageBox::information(
            this, Tr::tr("Cannot Delete Built-in Role"), Tr::tr("Built-in roles cannot be deleted."));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        Tr::tr("Delete Role"),
        Tr::tr("Are you sure you want to delete the role '%1'?").arg(role.name),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (AgentRolesManager::deleteRole(roleId)) {
            loadRoles();
        } else {
            QMessageBox::critical(
                this, Tr::tr("Error"), Tr::tr("Failed to delete role '%1'.").arg(role.name));
        }
    }
}

void AgentRolesWidget::onOpenRolesFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(AgentRolesManager::getConfigurationDirectory()));
}

} // namespace QodeAssist::Settings
