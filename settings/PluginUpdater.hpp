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
        QString changeLog;
        bool isUpdateAvailable = false;
    };

    explicit PluginUpdater(QObject *parent = nullptr);
    ~PluginUpdater() = default;

    void checkForUpdates();
    QString currentVersion() const;
    bool isUpdateAvailable() const;

signals:
    void updateCheckFinished(const UpdateInfo &info);

private:
    void handleUpdateResponse(QNetworkReply *reply);
    QString getUpdateUrl() const;

    QNetworkAccessManager *m_networkManager;
    UpdateInfo m_lastUpdateInfo;
    bool m_isCheckingUpdate = false;
};

} // namespace QodeAssist
