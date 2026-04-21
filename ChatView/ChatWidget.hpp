// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtQuickWidgets/QtQuickWidgets>

namespace QodeAssist::Chat {

class ChatWidget : public QQuickWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);
    ~ChatWidget() = default;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void scrollToBottom();

signals:
    void clearPressed();
};

} // namespace QodeAssist::Chat
