// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatWidget.hpp"

#include <QApplication>
#include <QMouseEvent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>

#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>

#include "plugin/QodeAssistConstants.hpp"
#include "SessionFileRegistry.hpp"
#include "skills/SkillsManager.hpp"

namespace QodeAssist::Chat {

ChatWidget::ChatWidget(
    QQmlEngine *engine,
    SessionFileRegistry *sessionFileRegistry,
    Skills::SkillsManager *skillsManager,
    bool registerOwnContext,
    QWidget *parent)
    : QQuickWidget{engine, parent}
{
    /// @note setup quick view content
    {
        auto context = new QQmlContext{engine, this};
        context->setContextProperty("sessionFileRegistry", sessionFileRegistry);
        context->setContextProperty("skillsManager", skillsManager);
        auto component = new QQmlComponent{engine, QUrl{"qrc:/qt/qml/ChatView/qml/RootItem.qml"}, this};
        auto rootItem = component->create(context);

        setContent(component->url(), component, rootItem);
    }
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setFocusPolicy(Qt::StrongFocus);

    setAttribute(Qt::WA_NoMousePropagation, true);

    if (registerOwnContext) {
        auto ideContext = new Core::IContext{this};
        ideContext->setWidget(this);
        ideContext->setContext(Core::Context{Constants::QODE_ASSIST_CHAT_CONTEXT});
        Core::ICore::addContextObject(ideContext);
    }
}

void ChatWidget::focusInEvent(QFocusEvent *event)
{
    QQuickWidget::focusInEvent(event);
    if (rootObject())
        QMetaObject::invokeMethod(rootObject(), "focusInput");
}

void ChatWidget::mousePressEvent(QMouseEvent *event)
{
    if (!hasFocus())
        setFocus(Qt::MouseFocusReason);

    QQuickWidget::mousePressEvent(event);
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
