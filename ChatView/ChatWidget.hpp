// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtQuickWidgets/QtQuickWidgets>

namespace QodeAssist::Chat {

class SessionFileRegistry;

class ChatWidget : public QQuickWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(
        QQmlEngine *engine,
        SessionFileRegistry *sessionFileRegistry,
        QWidget *parent = nullptr);
    ~ChatWidget() = default;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void scrollToBottom();
    Q_INVOKABLE void focusInput();

    void sendMessage();
    void clearSession();

    bool isChatFocused() const;

    static ChatWidget *focusedInstance();

signals:
    void clearPressed();
};

} // namespace QodeAssist::Chat
