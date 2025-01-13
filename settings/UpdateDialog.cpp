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

#include "UpdateDialog.hpp"

namespace QodeAssist {

UpdateDialog::UpdateDialog(QWidget *parent)
    : QDialog(parent)
    , m_updater(new PluginUpdater(this))
{
    setWindowTitle(tr("QodeAssist Update"));
    setMinimumWidth(400);
    setMinimumHeight(500);

    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(12);

    auto *supportLabel = new QLabel(
        tr("QodeAssist is an open-source project that helps\n"
           "developers write better code. If you find it useful, please"),
        this);
    supportLabel->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(supportLabel);

    auto *supportLink = new QLabel(
        tr("<a href='https://ko-fi.com/qodeassist' style='color: #0066cc;'>Support on Ko-fi "
           "☕</a>"),
        this);
    supportLink->setOpenExternalLinks(true);
    supportLink->setTextFormat(Qt::RichText);
    supportLink->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(supportLink);

    m_layout->addSpacing(20);

    m_titleLabel = new QLabel(tr("A new version of QodeAssist is available!"), this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(m_titleLabel);

    m_versionLabel = new QLabel(
        tr("Version %1 is now available - you have %2").arg(m_updater->currentVersion()), this);
    m_versionLabel->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(m_versionLabel);

    if (!m_changelogLabel) {
        m_changelogLabel = new QLabel(tr("Release Notes:"), this);
        m_layout->addWidget(m_changelogLabel);

        m_changelogText = new QTextEdit(this);
        m_changelogText->setReadOnly(true);
        m_changelogText->setMinimumHeight(100);
        m_layout->addWidget(m_changelogText);
    }

    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    m_layout->addWidget(m_progress);

    auto *buttonLayout = new QHBoxLayout;
    m_downloadButton = new QPushButton(tr("Download and Install"), this);
    m_downloadButton->setEnabled(false);
    buttonLayout->addWidget(m_downloadButton);

    m_closeButton = new QPushButton(tr("Close"), this);
    buttonLayout->addWidget(m_closeButton);

    m_layout->addLayout(buttonLayout);

    connect(m_updater, &PluginUpdater::updateCheckFinished, this, &UpdateDialog::handleUpdateInfo);
    connect(m_updater, &PluginUpdater::downloadProgress, this, &UpdateDialog::updateProgress);
    connect(m_updater, &PluginUpdater::downloadFinished, this, &UpdateDialog::handleDownloadFinished);
    connect(m_updater, &PluginUpdater::downloadError, this, &UpdateDialog::handleDownloadError);

    connect(m_downloadButton, &QPushButton::clicked, this, &UpdateDialog::startDownload);
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
    if (info.incompatibleIdeVersion) {
        m_titleLabel->setText(tr("Incompatible Qt Creator Version"));
        m_versionLabel->setText(tr("This update requires Qt Creator %1, current is %2.\n"
                                   "Please upgrade Qt Creator to use this version of QodeAssist.")
                                    .arg(info.targetIdeVersion, info.currentIdeVersion));
        m_downloadButton->setEnabled(false);
        return;
    }

    if (!info.isUpdateAvailable) {
        m_titleLabel->setText(tr("QodeAssist is up to date"));
        m_downloadButton->setEnabled(false);
        return;
    }

    m_titleLabel->setText(tr("A new version of QodeAssist is available!"));
    m_versionLabel->setText(tr("Version %1 is now available - you have %2")
                                .arg(info.version, m_updater->currentVersion()));

    if (!info.changeLog.isEmpty()) {
        if (!m_changelogLabel) {
            m_changelogLabel = new QLabel(tr("Release Notes:"), this);
            m_layout->insertWidget(2, m_changelogLabel);

            m_changelogText = new QTextEdit(this);
            m_changelogText->setReadOnly(true);
            m_changelogText->setMaximumHeight(200);
            m_layout->insertWidget(3, m_changelogText);
        }
        m_changelogText->setText(info.changeLog);
    }

    m_downloadButton->setEnabled(true);
    m_updateInfo = info;
}

void UpdateDialog::startDownload()
{
    m_downloadButton->setEnabled(false);
    m_progress->setVisible(true);
    m_updater->downloadUpdate(m_updateInfo.downloadUrl);
}

void UpdateDialog::updateProgress(qint64 received, qint64 total)
{
    m_progress->setMaximum(total);
    m_progress->setValue(received);
}

void UpdateDialog::handleDownloadFinished(const QString &path)
{
    m_progress->setVisible(false);
    QMessageBox::information(
        this,
        tr("Update Successful"),
        tr("Update has been downloaded and installed. "
           "Please restart Qt Creator to apply changes."));
    accept();
}

void UpdateDialog::handleDownloadError(const QString &error)
{
    m_progress->setVisible(false);
    m_downloadButton->setEnabled(true);
    QMessageBox::critical(this, tr("Update Error"), tr("Failed to update: %1").arg(error));
}

} // namespace QodeAssist
