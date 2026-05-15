// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatWidget.hpp"

#include <QApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>

#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>

#include "QodeAssistConstants.hpp"
#include "SessionFileRegistry.hpp"

namespace QodeAssist::Chat {

ChatWidget::ChatWidget(QQmlEngine *engine, SessionFileRegistry *sessionFileRegistry, QWidget *parent)
    : QQuickWidget{engine, parent}
{
    /// @note setup quick view content
    {
        auto context = new QQmlContext{engine, this};
        context->setContextProperty("sessionFileRegistry", sessionFileRegistry);
        auto component = new QQmlComponent{engine, QUrl{"qrc:/qt/qml/ChatView/qml/RootItem.qml"}, this};
        auto rootItem = component->create(context);

        setContent(component->url(), component, rootItem);
    }
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setFocusPolicy(Qt::StrongFocus);

    auto ideContext = new Core::IContext{this};
    ideContext->setWidget(this);
    ideContext->setContext(Core::Context{Constants::QODE_ASSIST_CHAT_CONTEXT});
    Core::ICore::addContextObject(ideContext);
}

void ChatWidget::clear()
{
    QMetaObject::invokeMethod(rootObject(), "clearChat");
}

void ChatWidget::scrollToBottom()
{
    QMetaObject::invokeMethod(rootObject(), "scrollToBottom");
}

void ChatWidget::focusInput()
{
    setFocus(Qt::OtherFocusReason);
    QMetaObject::invokeMethod(rootObject(), "focusInput");
}

bool ChatWidget::isChatFocused() const
{
    return hasFocus() || (rootObject() && rootObject()->hasActiveFocus());
}

void ChatWidget::sendMessage()
{
    QMetaObject::invokeMethod(rootObject(), "sendChatMessage");
}

void ChatWidget::clearSession()
{
    QMetaObject::invokeMethod(rootObject(), "clearChat");
}

ChatWidget *ChatWidget::focusedInstance()
{
    for (QWidget *widget = QApplication::focusWidget(); widget;
         widget = widget->parentWidget()) {
        if (auto chatWidget = qobject_cast<ChatWidget *>(widget))
            return chatWidget;
    }
    return nullptr;
}
} // namespace QodeAssist::Chat
