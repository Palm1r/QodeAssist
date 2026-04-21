// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PluginUpdater.hpp"

#include <coreplugin/coreconstants.h>
#include <coreplugin/coreplugin.h>
#include <extensionsystem/pluginmanager.h>
#include <extensionsystem/pluginspec.h>
#include <QJsonArray>
#include <QJsonDocument>

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
        emit updateCheckFinished(info);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();

    info.version = obj["tag_name"].toString();
    if (info.version.startsWith('v'))
        info.version.remove(0, 1);

    info.changeLog = obj["body"].toString();

    info.isUpdateAvailable = QVersionNumber::fromString(info.version)
                             > QVersionNumber::fromString(currentVersion());

    m_lastUpdateInfo = info;
    emit updateCheckFinished(info);
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

} // namespace QodeAssist
