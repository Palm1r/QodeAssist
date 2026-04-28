// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatWidget.hpp"

#include <QQmlContext>
#include <QQmlEngine>

namespace QodeAssist::Chat {

ChatWidget::ChatWidget(QQmlEngine* engine, QWidget *parent)
    : QQuickWidget{engine, parent}
{
    /// @note setup quick view content
    {
        auto context = new QQmlContext{engine, this};
        auto component = new QQmlComponent{engine, QUrl{"qrc:/qt/qml/ChatView/qml/RootItem.qml"}, this};
        auto rootItem = component->create(context);

        setContent(component->url(), component, rootItem);
    }
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
