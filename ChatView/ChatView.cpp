// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatView.hpp"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSettings>
#include <QVariantMap>

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <logger/Logger.hpp>

#include "ChatRootView.hpp"
#include "QodeAssistConstants.hpp"
#include "SessionFileRegistry.hpp"

namespace {
constexpr Qt::WindowFlags baseFlags = Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                                      | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint
                                      | Qt::WindowCloseButtonHint;
}

namespace QodeAssist::Chat {

ChatView::ChatView(QQmlEngine *engine, SessionFileRegistry *sessionFileRegistry)
    : QQuickView{engine, nullptr}
    , m_isPin(false)
{
    setTitle("QodeAssist Chat");
    /// @note setup quick view content
    {
        auto context = new QQmlContext{engine, this};
        context->setContextProperty("_chatview", this);
        context->setContextProperty("sessionFileRegistry", sessionFileRegistry);

        auto component = new QQmlComponent{engine, QUrl{"qrc:/qt/qml/ChatView/qml/RootItem.qml"}, this};
        auto rootItem = component->create(context);

        setContent(component->url(), component, rootItem);
    }

    if (auto rootView = qobject_cast<ChatRootView *>(rootObject())) {
        connect(
            rootView,
            &ChatRootView::closeHostRequested,
            this,
            &QWindow::close,
            Qt::QueuedConnection);
    }

    setResizeMode(QQuickView::SizeRootObjectToView);
    setMinimumSize({400, 300});
    setFlags(baseFlags);

    bindCommandShortcut("QodeAssist.CloseChatView", [this] { close(); });
    bindCommandShortcut(Constants::QODE_ASSIST_CHAT_SEND_MESSAGE, [this] {
        QMetaObject::invokeMethod(rootObject(), "sendChatMessage");
    });
    bindCommandShortcut(Constants::QODE_ASSIST_CHAT_CLEAR_SESSION, [this] {
        QMetaObject::invokeMethod(rootObject(), "clearChat");
    });

    restoreSettings();
}

void ChatView::bindCommandShortcut(Utils::Id commandId,
                                   const std::function<void()> &onActivated)
{
    auto command = Core::ActionManager::command(commandId);
    if (!command)
        return;

    auto shortcut = new QShortcut(command->keySequence(), this);
    connect(shortcut, &QShortcut::activated, this, onActivated);
    connect(command, &Core::Command::keySequenceChanged, shortcut, [command, shortcut]() {
        shortcut->setKey(command->keySequence());
    });
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
