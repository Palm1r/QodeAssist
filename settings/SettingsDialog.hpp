// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QWidget>

#include <coreplugin/icore.h>

namespace QodeAssist::Settings {

class SettingsDialog : public QDialog
{
public:
    explicit SettingsDialog(const QString &title, QWidget *parent = Core::ICore::dialogParent());

    QLabel *addLabel(const QString &text);
    QLineEdit *addInputField(const QString &labelText, const QString &value);
    void addSpacing(int space = 12);
    QHBoxLayout *buttonLayout();
    QVBoxLayout *mainLayout() const;

    QComboBox *addComboBox(
        const QStringList &items, const QString &currentText = QString(), bool editable = true);

private:
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_buttonLayout;
};

} // namespace QodeAssist::Settings
