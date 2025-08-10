/*
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include "ChatView.hpp"

#include <QQmlContext>
#include <QQmlEngine>
#include <QSettings>
#include <QVariantMap>

#include <logger/Logger.hpp>

namespace {
constexpr Qt::WindowFlags baseFlags = Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                                      | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint
                                      | Qt::WindowCloseButtonHint;
}

namespace QodeAssist::Chat {

ChatView::ChatView()
    : m_isPin(false)
{
    setTitle("QodeAssist Chat");
    engine()->rootContext()->setContextProperty("_chatview", this);
    setSource(QUrl("qrc:/qt/qml/ChatView/qml/RootItem.qml"));
    setResizeMode(QQuickView::SizeRootObjectToView);
    setMinimumSize({400, 300});
    setFlags(baseFlags);

    restoreSettings();
}

void ChatView::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void ChatView::saveSettings()
{
    QSettings settings;
    settings.setValue("QodeAssist/ChatView/geometry", geometry());
    settings.setValue("QodeAssist/ChatView/pinned", m_isPin);
}

void ChatView::restoreSettings()
{
    QSettings settings;
    const QRect savedGeometry
        = settings.value("QodeAssist/ChatView/geometry", QRect(100, 100, 800, 600)).toRect();
    setGeometry(savedGeometry);

    const bool pinned = settings.value("QodeAssist/ChatView/pinned", false).toBool();
    setIsPin(pinned);
}

bool ChatView::isPin() const
{
    return m_isPin;
}

void ChatView::setIsPin(bool newIsPin)
{
    if (m_isPin == newIsPin)
        return;
    m_isPin = newIsPin;

    if (m_isPin) {
        setFlags(baseFlags | Qt::WindowStaysOnTopHint);
    } else {
        setFlags(baseFlags);
    }

    emit isPinChanged();
}

} // namespace QodeAssist::Chat
