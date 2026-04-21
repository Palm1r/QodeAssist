// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsDialog.hpp"

namespace QodeAssist::Settings {

SettingsDialog::SettingsDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_buttonLayout(nullptr)
{
    setWindowTitle(title);
    m_mainLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
}

QLabel *SettingsDialog::addLabel(const QString &text)
{
    auto label = new QLabel(text, this);
    label->setWordWrap(true);
    label->setMinimumWidth(300);
    m_mainLayout->addWidget(label);
    return label;
}

QLineEdit *SettingsDialog::addInputField(const QString &labelText, const QString &value)
{
    auto inputLayout = new QGridLayout;
    auto inputLabel = new QLabel(labelText, this);
    auto inputField = new QLineEdit(value, this);
    inputField->setMinimumWidth(200);

    inputLayout->addWidget(inputLabel, 0, 0);
    inputLayout->addWidget(inputField, 0, 1);
    inputLayout->setColumnStretch(1, 1);
    m_mainLayout->addLayout(inputLayout);

    return inputField;
}

void SettingsDialog::addSpacing(int space)
{
    m_mainLayout->addSpacing(space);
}

QHBoxLayout *SettingsDialog::buttonLayout()
{
    if (!m_buttonLayout) {
        m_buttonLayout = new QHBoxLayout;
        m_buttonLayout->addStretch();
        m_mainLayout->addLayout(m_buttonLayout);
    }
    return m_buttonLayout;
}

QComboBox *SettingsDialog::addComboBox(
    const QStringList &items, const QString &currentText, bool editable)
{
    auto comboBox = new QComboBox(this);
    comboBox->addItems(items);
    comboBox->setCurrentText(currentText);
    comboBox->setMinimumWidth(300);
    comboBox->setEditable(editable);
    m_mainLayout->addWidget(comboBox);
    return comboBox;
}

QVBoxLayout *SettingsDialog::mainLayout() const
{
    return m_mainLayout;
}

} // namespace QodeAssist::Settings
