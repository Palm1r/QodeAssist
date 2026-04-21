// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qobject.h>
#include <qqmlintegration.h>

namespace QodeAssist::Chat {

class ChatUtils : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ChatUtils)

public:
    explicit ChatUtils(QObject *parent = nullptr)
        : QObject(parent) {};

    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE QString getSafeMarkdownText(const QString &text) const;
};

} // namespace QodeAssist::Chat
