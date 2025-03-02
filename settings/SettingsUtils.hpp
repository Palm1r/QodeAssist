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

#include <utils/aspects.h>
#include <utils/layoutbuilder.h>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QtCore/qtimer.h>

namespace QodeAssist::Settings {

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

template<typename AspectType>
void resetAspect(AspectType &aspect)
{
    aspect.setVolatileValue(aspect.defaultValue());
}

inline void initStringAspect(
    Utils::StringAspect &aspect,
    const Utils::Key &settingsKey,
    const QString &labelText,
    const QString &defaultValue)
{
    aspect.setSettingsKey(settingsKey);
    aspect.setLabelText(labelText);
    aspect.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    aspect.setDefaultValue(defaultValue);
}

} // namespace QodeAssist::Settings
