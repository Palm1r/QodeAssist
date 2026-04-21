// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
