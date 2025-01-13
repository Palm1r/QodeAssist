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

#include <coreplugin/icore.h>
#include <coreplugin/plugininstallwizard.h>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QVersionNumber>

namespace QodeAssist {

class PluginUpdater : public QObject
{
    Q_OBJECT
public:
    struct UpdateInfo
    {
        QString version;
        QString downloadUrl;
        QString changeLog;
        QString fileName;
        bool isUpdateAvailable;
        bool incompatibleIdeVersion{false};
        QString targetIdeVersion;
        QString currentIdeVersion;
    };

    explicit PluginUpdater(QObject *parent = nullptr);
    ~PluginUpdater() = default;

    void checkForUpdates();
    void downloadUpdate(const QString &url);
    QString currentVersion() const;
    bool isUpdateAvailable() const;

signals:
    void updateCheckFinished(const UpdateInfo &info);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString &filePath);
    void downloadError(const QString &error);

private:
    void handleUpdateResponse(QNetworkReply *reply);
    void handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void handleDownloadFinished();
    QString getUpdateUrl() const;

    QNetworkAccessManager *m_networkManager;
    UpdateInfo m_lastUpdateInfo;
    bool m_isCheckingUpdate{false};
};

} // namespace QodeAssist
