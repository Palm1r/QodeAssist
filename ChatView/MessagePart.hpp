// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QtQmlIntegration>

#include "ChatData.hpp"

namespace QodeAssist::Chat {

class MessagePart
{
    Q_GADGET
    Q_PROPERTY(MessagePartType type MEMBER type CONSTANT FINAL)
    Q_PROPERTY(QString text MEMBER text CONSTANT FINAL)
    Q_PROPERTY(QString language MEMBER language CONSTANT FINAL)
    Q_PROPERTY(QString imageData MEMBER imageData CONSTANT FINAL)
    Q_PROPERTY(QString mediaType MEMBER mediaType CONSTANT FINAL)
    QML_VALUE_TYPE(messagePart)
public:
    MessagePartType type;
    QString text;
    QString language;
    QString imageData;    // Base64 data or URL
    QString mediaType;    // e.g., "image/png", "image/jpeg"
};

} // namespace QodeAssist::Chat
