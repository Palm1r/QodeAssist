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

#include "AddCustomInstructionDialog.hpp"
#include "QodeAssisttr.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace QodeAssist {

AddCustomInstructionDialog::AddCustomInstructionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(Tr::tr("Add Custom Instruction"));
    setupUi();
    resize(500, 400);
}

AddCustomInstructionDialog::AddCustomInstructionDialog(const CustomInstruction &instruction, QWidget *parent)
    : QDialog(parent)
    , m_instruction(instruction)
{
    setWindowTitle(Tr::tr("Edit Custom Instruction"));
    setupUi();
    m_nameEdit->setText(instruction.name);
    m_bodyEdit->setPlainText(instruction.body);
    m_defaultCheckBox->setChecked(instruction.isDefault);
    resize(500, 400);
}

void AddCustomInstructionDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    QFormLayout *formLayout = new QFormLayout();
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(Tr::tr("Enter instruction name..."));
    formLayout->addRow(Tr::tr("Name:"), m_nameEdit);

    mainLayout->addLayout(formLayout);

    QLabel *bodyLabel = new QLabel(Tr::tr("Instruction Body:"), this);
    mainLayout->addWidget(bodyLabel);

    m_bodyEdit = new QPlainTextEdit(this);
    m_bodyEdit->setPlaceholderText(
        Tr::tr("Enter the refactoring instruction that will be sent to the LLM..."));
    mainLayout->addWidget(m_bodyEdit);

    m_defaultCheckBox = new QCheckBox(Tr::tr("Set as default instruction"), this);
    m_defaultCheckBox->setToolTip(
        Tr::tr("This instruction will be automatically selected when opening Quick Refactor dialog"));
    mainLayout->addWidget(m_defaultCheckBox);

    QDialogButtonBox *buttonBox
        = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (m_nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, Tr::tr("Invalid Input"), Tr::tr("Instruction name cannot be empty."));
            return;
        }
        if (m_bodyEdit->toPlainText().trimmed().isEmpty()) {
            QMessageBox::warning(this, Tr::tr("Invalid Input"), Tr::tr("Instruction body cannot be empty."));
            return;
        }
        accept();
    });

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

CustomInstruction AddCustomInstructionDialog::getInstruction() const
{
    CustomInstruction instruction = m_instruction;
    instruction.name = m_nameEdit->text().trimmed();
    instruction.body = m_bodyEdit->toPlainText().trimmed();
    instruction.isDefault = m_defaultCheckBox->isChecked();
    return instruction;
}

} // namespace QodeAssist

