// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
