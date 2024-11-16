/* 
 * Copyright (C) 2024 Petr Mironychev
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
