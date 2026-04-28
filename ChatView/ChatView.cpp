// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatView.hpp"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSettings>
#include <QVariantMap>

#include <coreplugin/actionmanager/actionmanager.h>
#include <logger/Logger.hpp>

namespace {
constexpr Qt::WindowFlags baseFlags = Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                                      | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint
                                      | Qt::WindowCloseButtonHint;
}

namespace QodeAssist::Chat {

ChatView::ChatView(QQmlEngine* engine)
    : QQuickView{engine, nullptr}
    , m_isPin(false)
{
    setTitle("QodeAssist Chat");
    /// @note setup quick view content
    {
        auto context = new QQmlContext{engine, this};
        context->setContextProperty("_chatview", this);

        auto component = new QQmlComponent{engine, QUrl{"qrc:/qt/qml/ChatView/qml/RootItem.qml"}, this};
        auto rootItem = component->create(context);

        setContent(component->url(), component, rootItem);
    }
    setResizeMode(QQuickView::SizeRootObjectToView);
    setMinimumSize({400, 300});
    setFlags(baseFlags);

    if (auto action = Core::ActionManager::command("QodeAssist.CloseChatView")) {
        m_closeShortcut = new QShortcut(action->keySequence(), this);
        connect(m_closeShortcut, &QShortcut::activated, this, &QQuickView::close);

        connect(action, &Core::Command::keySequenceChanged, this, [action, this]() {
            if (m_closeShortcut) {
                m_closeShortcut->setKey(action->keySequence());
            }
        });
    }

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
