// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QStringList>

class QComboBox;
class QLineEdit;

namespace QodeAssist::Settings {

class NewProviderDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewProviderDialog(const QStringList &types, QWidget *parent = nullptr);

    QString providerType() const;
    QString providerName() const;
    QString url() const;
    QString description() const;
    QString apiKey() const;

private:
    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QLineEdit *m_descriptionEdit = nullptr;
    QLineEdit *m_apiKeyEdit = nullptr;
};

} // namespace QodeAssist::Settings
