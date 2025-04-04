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

#include "ChatWidget.hpp"

#include <QQmlContext>
#include <QQmlEngine>

namespace QodeAssist::Chat {

ChatWidget::ChatWidget(QWidget *parent)
    : QQuickWidget(parent)
{
    setSource(QUrl("qrc:/qt/qml/ChatView/qml/RootItem.qml"));
    setResizeMode(QQuickWidget::SizeRootObjectToView);
}

void ChatWidget::clear()
{
    QMetaObject::invokeMethod(rootObject(), "clearChat");
}

void ChatWidget::scrollToBottom()
{
    QMetaObject::invokeMethod(rootObject(), "scrollToBottom");
}
} // namespace QodeAssist::Chat
