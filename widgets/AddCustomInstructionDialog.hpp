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
#include <QString>

#include "CustomInstructionsManager.hpp"

class QLineEdit;
class QPlainTextEdit;
class QCheckBox;

namespace QodeAssist {

class AddCustomInstructionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddCustomInstructionDialog(QWidget *parent = nullptr);
    explicit AddCustomInstructionDialog(const CustomInstruction &instruction, QWidget *parent = nullptr);
    ~AddCustomInstructionDialog() override = default;

    CustomInstruction getInstruction() const;

private:
    void setupUi();
    
    QLineEdit *m_nameEdit;
    QPlainTextEdit *m_bodyEdit;
    QCheckBox *m_defaultCheckBox;
    CustomInstruction m_instruction;
};

} // namespace QodeAssist

