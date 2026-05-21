// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NewProviderDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace QodeAssist::Settings {

NewProviderDialog::NewProviderDialog(const QStringList &types, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New provider"));
    setMinimumWidth(520);

    auto *intro = new QLabel(
        tr("A provider binds a client API to a URL and API key. "
           "Agents reference providers by name."),
        this);
    intro->setWordWrap(true);
    QPalette ip = intro->palette();
    ip.setColor(QPalette::WindowText, ip.color(QPalette::Mid));
    intro->setPalette(ip);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems(types);
    m_typeCombo->setEditable(false);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Shown in the providers list and referenced by agents."));

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(QStringLiteral("https://api.example.com"));

    m_descriptionEdit = new QLineEdit(this);
    m_descriptionEdit->setPlaceholderText(tr("Optional — what this provider is for."));

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(tr("(stored — leave blank to set later)"));

    auto *form = new QFormLayout;
    form->addRow(tr("Client API:"), m_typeCombo);
    form->addRow(tr("Name:"), m_nameEdit);
    form->addRow(tr("URL:"), m_urlEdit);
    form->addRow(tr("Description:"), m_descriptionEdit);
    form->addRow(tr("API key:"), m_apiKeyEdit);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Create"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addWidget(intro);
    root->addLayout(form);
    root->addWidget(buttons);

    connect(m_typeCombo, &QComboBox::currentTextChanged, this, [this](const QString &type) {
        if (m_nameEdit->text().isEmpty())
            m_nameEdit->setText(type);
    });

    if (!types.isEmpty() && m_nameEdit->text().isEmpty())
        m_nameEdit->setText(types.front());
}

QString NewProviderDialog::providerType() const { return m_typeCombo->currentText(); }
QString NewProviderDialog::providerName() const { return m_nameEdit->text().trimmed(); }
QString NewProviderDialog::url() const { return m_urlEdit->text().trimmed(); }
QString NewProviderDialog::description() const { return m_descriptionEdit->text().trimmed(); }
QString NewProviderDialog::apiKey() const { return m_apiKeyEdit->text(); }

} // namespace QodeAssist::Settings
