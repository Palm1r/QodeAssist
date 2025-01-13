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

#pragma once

#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QToolButton>

namespace QodeAssist {

class UpdateStatusWidget : public QFrame
{
    Q_OBJECT
public:
    explicit UpdateStatusWidget(QWidget *parent = nullptr);

    void setDefaultAction(QAction *action);
    void showUpdateAvailable(const QString &version);
    void hideUpdateInfo();

    QPushButton *updateButton() const;

private:
    QToolButton *m_actionButton;
    QLabel *m_versionLabel;
    QPushButton *m_updateButton;
};
} // namespace QodeAssist
