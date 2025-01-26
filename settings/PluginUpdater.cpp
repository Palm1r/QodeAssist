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

#include "PluginUpdater.hpp"

#include <coreplugin/coreconstants.h>
#include <coreplugin/coreplugin.h>
#include <extensionsystem/pluginmanager.h>
#include <extensionsystem/pluginspec.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace QodeAssist {

PluginUpdater::PluginUpdater(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{}

void PluginUpdater::checkForUpdates()
{
    if (m_isCheckingUpdate)
        return;

    m_isCheckingUpdate = true;
    QNetworkRequest request((QUrl(getUpdateUrl())));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleUpdateResponse(reply);
        m_isCheckingUpdate = false;
        reply->deleteLater();
    });
}

void PluginUpdater::handleUpdateResponse(QNetworkReply *reply)
{
    UpdateInfo info;

    if (reply->error() != QNetworkReply::NoError) {
        emit downloadError(reply->errorString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();

    info.version = obj["tag_name"].toString();
    if (info.version.startsWith('v'))
        info.version.remove(0, 1);

    QString qtcVersionStr = Core::ICore::versionString().split(' ').last();
    QVersionNumber qtcVersion = QVersionNumber::fromString(qtcVersionStr);
    info.currentIdeVersion = qtcVersionStr;

    auto assets = obj["assets"].toArray();
    for (const auto &asset : assets) {
        QString name = asset.toObject()["name"].toString();

        if (name.startsWith("QodeAssist-")) {
            QString assetVersionStr = name.section('-', 1, 1);
            QVersionNumber assetVersion = QVersionNumber::fromString(assetVersionStr);
            info.targetIdeVersion = assetVersionStr;

            if (assetVersion != qtcVersion) {
                continue;
            }

#if defined(Q_OS_WIN)
            if (name.contains("Windows"))
#elif defined(Q_OS_MACOS)
            if (name.contains("macOS"))
#else
            if (name.contains("Linux") && !name.contains("experimental"))
#endif
            {
                info.downloadUrl = asset.toObject()["browser_download_url"].toString();
                info.fileName = name;
                break;
            }
        }
    }

    if (info.downloadUrl.isEmpty()) {
        info.incompatibleIdeVersion = true;
        emit updateCheckFinished(info);
        return;
    }

    info.changeLog = obj["body"].toString();
    info.isUpdateAvailable = QVersionNumber::fromString(info.version)
                             > QVersionNumber::fromString(currentVersion());

    m_lastUpdateInfo = info;
    emit updateCheckFinished(info);
}

void PluginUpdater::downloadUpdate(const QString &url)
{
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this, &PluginUpdater::handleDownloadProgress);
    connect(reply, &QNetworkReply::finished, this, &PluginUpdater::handleDownloadFinished);
}

QString PluginUpdater::currentVersion() const
{
    const auto pluginSpecs = ExtensionSystem::PluginManager::plugins();
    for (const ExtensionSystem::PluginSpec *spec : pluginSpecs) {
        if (spec->name() == QLatin1String("QodeAssist"))
            return spec->version();
    }
    return QString();
}

bool PluginUpdater::isUpdateAvailable() const
{
    return m_lastUpdateInfo.isUpdateAvailable;
}

QString PluginUpdater::getUpdateUrl() const
{
    return "https://api.github.com/repos/Palm1r/qodeassist/releases/latest";
}

void PluginUpdater::handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void PluginUpdater::handleDownloadFinished()
{
    auto reply = qobject_cast<QNetworkReply *>(sender());
    QTC_ASSERT(reply, return);

    if (reply->error() != QNetworkReply::NoError) {
        emit downloadError(reply->errorString());
        reply->deleteLater();
        return;
    }

    QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
                           + QDir::separator() + "QodeAssist_v" + m_lastUpdateInfo.version;
    QDir().mkpath(downloadPath);

    QString filePath = downloadPath + QDir::separator() + m_lastUpdateInfo.fileName;

    if (QFile::exists(filePath)) {
        emit downloadError(tr("Update file already exists: %1").arg(filePath));
        reply->deleteLater();
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit downloadError(tr("Could not save the update file"));
        reply->deleteLater();
        return;
    }

    file.write(reply->readAll());
    file.close();

    emit downloadFinished(filePath);
    reply->deleteLater();
}

} // namespace QodeAssist
