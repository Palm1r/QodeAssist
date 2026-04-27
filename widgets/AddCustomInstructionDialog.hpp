// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

