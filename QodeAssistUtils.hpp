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

#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <coreplugin/messagemanager.h>
#include <utils/qtcassert.h>

namespace QodeAssist {

inline bool &loggingEnabled()
{
    static bool enabled = false;
    return enabled;
}

inline void setLoggingEnabled(bool enable)
{
    loggingEnabled() = enable;
}

inline void logMessage(const QString &message, bool silent = true)
{
    if (!loggingEnabled())
        return;

    const QString prefixedMessage = QLatin1String("[Qode Assist] ") + message;
    if (silent) {
        Core::MessageManager::writeSilently(prefixedMessage);
    } else {
        Core::MessageManager::writeFlashing(prefixedMessage);
    }
}

inline void logMessages(const QStringList &messages, bool silent = true)
{
    if (!loggingEnabled())
        return;

    QStringList prefixedMessages;
    qDebug() << prefixedMessages;

    for (const QString &message : messages) {
        prefixedMessages << (QLatin1String("[Qode Assist] ") + message);
    }
    if (silent) {
        Core::MessageManager::writeSilently(prefixedMessages);
    } else {
        Core::MessageManager::writeFlashing(prefixedMessages);
    }
}

inline bool pingUrl(const QUrl &url, int timeout = 5000)
{
    if (!url.isValid()) {
        return false;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, true);

    QScopedPointer<QNetworkReply> reply(manager.get(request));

    QTimer timer;
    timer.setSingleShot(true);

    QEventLoop loop;
    QObject::connect(reply.data(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(timeout);
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
        return (reply->error() == QNetworkReply::NoError);
    } else {
        QObject::disconnect(reply.data(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
        reply->abort();
        return false;
    }
}

} // namespace QodeAssist
