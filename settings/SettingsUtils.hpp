// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
