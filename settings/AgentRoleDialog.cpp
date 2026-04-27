// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AgentRoleDialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace QodeAssist::Settings {

AgentRoleDialog::AgentRoleDialog(Action action, QWidget *parent)
    : QDialog{parent}
    , m_action{action}
{
    auto getTitle = [](Action action) {
        switch(action)
        {
        case Action::Add:
            return tr("Add Agent Role");
        case Action::Duplicate:
            return tr("Duplicate Agent Role");
        case Action::Edit:
            return tr("Edit Agent Role");
        }
    };

    setWindowTitle(getTitle(action));
    setupUI();
}

void AgentRoleDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout();

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("e.g., Developer, Code Reviewer"));
    formLayout->addRow(tr("Name:"), m_nameEdit);

    m_idEdit = new QLineEdit(this);
    m_idEdit->setPlaceholderText(tr("e.g., developer, code_reviewer"));
    formLayout->addRow(tr("ID:"), m_idEdit);

    m_descriptionEdit = new QTextEdit(this);
    m_descriptionEdit->setPlaceholderText(tr("Brief description of this role..."));
    m_descriptionEdit->setMaximumHeight(80);
    formLayout->addRow(tr("Description:"), m_descriptionEdit);

    mainLayout->addLayout(formLayout);

    auto *promptLabel = new QLabel(tr("System Prompt:"), this);
    mainLayout->addWidget(promptLabel);

    m_systemPromptEdit = new QTextEdit(this);
    m_systemPromptEdit->setPlaceholderText(
        tr("You are an expert in...\n\nYour role is to:\n- Task 1\n- Task 2\n- Task 3"));
    mainLayout->addWidget(m_systemPromptEdit);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &AgentRoleDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &AgentRoleDialog::reject);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &AgentRoleDialog::validateInput);
    connect(m_idEdit, &QLineEdit::textChanged, this, &AgentRoleDialog::validateInput);
    connect(m_systemPromptEdit, &QTextEdit::textChanged, this, &AgentRoleDialog::validateInput);

    if (m_action == Action::Edit) {
        m_idEdit->setEnabled(false);
        m_idEdit->setToolTip(tr("ID cannot be changed for existing roles"));
    }

    setMinimumSize(600, 500);
    validateInput();
}

void AgentRoleDialog::validateInput()
{
    bool valid = !m_nameEdit->text().trimmed().isEmpty()
                 && !m_idEdit->text().trimmed().isEmpty()
                 && !m_systemPromptEdit->toPlainText().trimmed().isEmpty();

    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

AgentRole AgentRoleDialog::getRole() const
{
    return AgentRole{
        m_idEdit->text().trimmed(),
        m_nameEdit->text().trimmed(),
        m_descriptionEdit->toPlainText().trimmed(),
        m_systemPromptEdit->toPlainText().trimmed(),
        false};
}

void AgentRoleDialog::setRole(const AgentRole &role)
{
    m_idEdit->setText(role.id);
    m_nameEdit->setText(role.name);
    m_descriptionEdit->setPlainText(role.description);
    m_systemPromptEdit->setPlainText(role.systemPrompt);
}

} // namespace QodeAssist::Settings
