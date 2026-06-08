// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "UpdateDialog.hpp"

#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>
#include <extensionsystem/pluginspec.h>
#include <QClipboard>
#include <QDesktopServices>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace QodeAssist {

UpdateDialog::UpdateDialog(QWidget *parent)
    : QDialog(parent)
    , m_updater(new PluginUpdater(this))
{
    setWindowTitle(tr("QodeAssist Update"));
    setFixedSize(700, 720);

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

    auto *paypalLink = new QLabel(
        "<a href='https://www.paypal.com/paypalme/palm1r' style='color: #0066cc;'>Support via PayPal "
        "💳</a>",
        this);
    paypalLink->setOpenExternalLinks(true);
    paypalLink->setTextFormat(Qt::RichText);
    paypalLink->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(paypalLink);

    m_layout->addSpacing(20);

    auto *registryGroup = new QGroupBox(tr("Install via Extension Registry (recommended)"), this);
    auto *registryLayout = new QVBoxLayout(registryGroup);
    registryLayout->setSpacing(10);

    auto *registryInfoLabel = new QLabel(
        tr("In Qt Creator open Extensions → Browser tab, enable \"Use External Repository\", "
           "add one of the URLs below and click Apply to install QodeAssist. Updates are then "
           "installed from the same screen."),
        registryGroup);
    registryInfoLabel->setWordWrap(true);
    registryLayout->addWidget(registryInfoLabel);

    const auto addRegistryRow = [&](const QString &description, const QString &url) {
        auto *card = new QFrame(registryGroup);
        card->setFrameShape(QFrame::StyledPanel);
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(10, 8, 10, 8);
        cardLayout->setSpacing(10);

        auto *textLayout = new QVBoxLayout;
        textLayout->setSpacing(2);

        auto *desc = new QLabel(description, card);
        desc->setStyleSheet("font-weight: bold;");
        textLayout->addWidget(desc);

        auto *link = new QLabel(
            QString("<a href='%1' style='color: #0066cc;'>%1</a>").arg(url), card);
        link->setOpenExternalLinks(true);
        link->setTextFormat(Qt::RichText);
        link->setTextInteractionFlags(Qt::TextBrowserInteraction);
        link->setWordWrap(true);
        textLayout->addWidget(link);

        cardLayout->addLayout(textLayout, 1);

        auto *copyButton = new QPushButton(tr("Copy"), card);
        copyButton->setMaximumWidth(70);
        connect(copyButton, &QPushButton::clicked, this, [url]() {
            QGuiApplication::clipboard()->setText(url);
        });
        cardLayout->addWidget(copyButton, 0, Qt::AlignVCenter);

        registryLayout->addWidget(card);
    };

    addRegistryRow(
        tr("Latest (for the newest Qt Creator, always up to date)"),
        "https://github.com/Palm1r/extension-registry/archive/refs/heads/qodeassist.tar.gz");
    addRegistryRow(
        tr("Only for Qt Creator %1").arg(QODEASSIST_QT_CREATOR_VERSION_MAJOR),
        QString("https://github.com/Palm1r/extension-registry/archive/refs/heads/"
                "qodeassist-qtc%1.tar.gz")
            .arg(QODEASSIST_QT_CREATOR_VERSION_MAJOR));

    m_layout->addWidget(registryGroup);

    auto *updaterGroup = new QGroupBox(tr("Alternative: QodeAssistUpdater"), this);
    auto *updaterLayout = new QVBoxLayout(updaterGroup);
    updaterLayout->setSpacing(10);

    auto *updaterInfoLabel = new QLabel(
        tr("A standalone tool for installing and updating the plugin."), updaterGroup);
    updaterInfoLabel->setWordWrap(true);
    updaterLayout->addWidget(updaterInfoLabel);

    m_buttonOpenUpdaterRelease = new QPushButton(tr("Download QodeAssistUpdater"), updaterGroup);
    m_buttonOpenUpdaterRelease->setMaximumWidth(250);
    auto *updaterButtonLayout = new QHBoxLayout;
    updaterButtonLayout->addStretch();
    updaterButtonLayout->addWidget(m_buttonOpenUpdaterRelease);
    updaterButtonLayout->addStretch();
    updaterLayout->addLayout(updaterButtonLayout);

    m_layout->addWidget(updaterGroup);

    m_layout->addSpacing(10);

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
