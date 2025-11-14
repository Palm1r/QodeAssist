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
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "PluginUpdater.hpp"

namespace QodeAssist {

class UpdateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UpdateDialog(QWidget *parent = nullptr);

    static void checkForUpdatesAndShow(QWidget *parent = nullptr);

private slots:
    void handleUpdateInfo(const PluginUpdater::UpdateInfo &info);
    void openReleasePage();
    void openPluginFolder();
    void openUpdaterReleasePage();

private:
    PluginUpdater *m_updater;
    QVBoxLayout *m_layout;
    QLabel *m_titleLabel;
    QLabel *m_versionLabel;
    QLabel *m_changelogLabel;
    QTextEdit *m_changelogText;
    QPushButton *m_buttonOpenReleasePage;
    QPushButton *m_buttonOpenPluginFolder;
    QPushButton *m_buttonOpenUpdaterRelease;
    QPushButton *m_closeButton;
    PluginUpdater::UpdateInfo m_updateInfo;
};

} // namespace QodeAssist
