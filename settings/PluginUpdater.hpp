// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
