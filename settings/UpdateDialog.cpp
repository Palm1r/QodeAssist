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

#include "UpdateDialog.hpp"

#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>
#include <extensionsystem/pluginspec.h>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace QodeAssist {

UpdateDialog::UpdateDialog(QWidget *parent)
    : QDialog(parent)
    , m_updater(new PluginUpdater(this))
{
    setWindowTitle(tr("QodeAssist Update"));
    setMinimumWidth(400);
    setMinimumHeight(300);

    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(12);

    auto *supportLabel = new QLabel(
        tr("QodeAssist is an open-source project that helps\n"
           "developers write better code. If you find it useful, please"),
        this);
    supportLabel->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(supportLabel);

    auto *supportLink = new QLabel(
        "<a href='https://ko-fi.com/qodeassist' style='color: #0066cc;'>Support on Ko-fi "
           "☕</a>",
        this);
    supportLink->setOpenExternalLinks(true);
    supportLink->setTextFormat(Qt::RichText);
    supportLink->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(supportLink);
    auto *githubSupportLink = new QLabel(
        "<a "
        "href='https://github.com/Palm1r/"
        "QodeAssist?tab=readme-ov-file#support-the-development-of-qodeassist' style='color: #0066cc;' > Support page on github </a>",
        this);
    githubSupportLink->setOpenExternalLinks(true);
    githubSupportLink->setTextFormat(Qt::RichText);
    githubSupportLink->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(githubSupportLink);

    m_layout->addSpacing(20);

    auto *updaterInfoLabel = new QLabel(
        tr("QodeAssistUpdater - convenient tool for plugin installation and updates"),
        this);
    updaterInfoLabel->setAlignment(Qt::AlignCenter);
    updaterInfoLabel->setWordWrap(true);
    m_layout->addWidget(updaterInfoLabel);

    m_buttonOpenUpdaterRelease = new QPushButton(tr("Download QodeAssistUpdater"), this);
    m_buttonOpenUpdaterRelease->setMaximumWidth(250);
    auto *updaterButtonLayout = new QHBoxLayout;
    updaterButtonLayout->addStretch();
    updaterButtonLayout->addWidget(m_buttonOpenUpdaterRelease);
    updaterButtonLayout->addStretch();
    m_layout->addLayout(updaterButtonLayout);

    m_layout->addSpacing(20);

    m_titleLabel = new QLabel(tr("A new version of QodeAssist is available!"), this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(m_titleLabel);

    m_versionLabel = new QLabel(
        tr("Version %1 is now available - you have %2").arg("", m_updater->currentVersion()), this);
    m_versionLabel->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(m_versionLabel);

    m_changelogLabel = new QLabel(tr("Release Notes:"), this);
    m_layout->addWidget(m_changelogLabel);

    m_changelogText = new QTextEdit(this);
    m_changelogText->setReadOnly(true);
    m_changelogText->setMinimumHeight(100);
    m_layout->addWidget(m_changelogText);

    auto *buttonLayout = new QHBoxLayout;
    m_buttonOpenReleasePage = new QPushButton(tr("Open Release Page"), this);
    buttonLayout->addWidget(m_buttonOpenReleasePage);

    m_buttonOpenPluginFolder = new QPushButton(tr("Open Plugin Folder"), this);
    buttonLayout->addWidget(m_buttonOpenPluginFolder);

    m_closeButton = new QPushButton(tr("Close"), this);
    buttonLayout->addWidget(m_closeButton);

    m_layout->addLayout(buttonLayout);

    connect(m_updater, &PluginUpdater::updateCheckFinished, this, &UpdateDialog::handleUpdateInfo);
    connect(m_buttonOpenReleasePage, &QPushButton::clicked, this, &UpdateDialog::openReleasePage);
    connect(m_buttonOpenPluginFolder, &QPushButton::clicked, this, &UpdateDialog::openPluginFolder);
    connect(m_buttonOpenUpdaterRelease, &QPushButton::clicked, this, &UpdateDialog::openUpdaterReleasePage);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);

    m_updater->checkForUpdates();
}

void UpdateDialog::checkForUpdatesAndShow(QWidget *parent)
{
    auto *dialog = new UpdateDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void UpdateDialog::handleUpdateInfo(const PluginUpdater::UpdateInfo &info)
{
    m_updateInfo = info;

    if (!info.isUpdateAvailable) {
        m_titleLabel->setText(tr("QodeAssist is up to date"));
        m_versionLabel->setText(
            tr("You are using the latest version: %1").arg(m_updater->currentVersion()));
        return;
    }

    m_titleLabel->setText(tr("A new version of QodeAssist is available!"));
    m_versionLabel->setText(tr("Version %1 is now available - you have %2")
                                .arg(info.version, m_updater->currentVersion()));

    if (!info.changeLog.isEmpty()) {
        m_changelogText->setText(info.changeLog);
    } else {
        m_changelogText->setText(
            tr("No release notes available. Check the release page for more information."));
    }
}

void UpdateDialog::openReleasePage()
{
    QDesktopServices::openUrl(QUrl("https://github.com/Palm1r/QodeAssist/releases/latest"));
    accept();
}

void UpdateDialog::openPluginFolder()
{
    const auto pluginSpecs = ExtensionSystem::PluginManager::plugins();
    for (const ExtensionSystem::PluginSpec *spec : pluginSpecs) {
        if (spec->name() == QLatin1String("QodeAssist")) {
            const auto pluginPath = spec->filePath().path();
            QFileInfo fileInfo(pluginPath);
            QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
            break;
        }
    }
    accept();
}

void UpdateDialog::openUpdaterReleasePage()
{
    QDesktopServices::openUrl(QUrl("https://github.com/Palm1r/QodeAssistUpdater"));
}

} // namespace QodeAssist
