/*
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <qobject.h>
#include <qqmlintegration.h>

namespace QodeAssist::Chat {
Q_NAMESPACE

class MessagePart
{
    Q_GADGET
    Q_PROPERTY(PartType type MEMBER type CONSTANT FINAL)
    Q_PROPERTY(QString text MEMBER text CONSTANT FINAL)
    Q_PROPERTY(QString language MEMBER language CONSTANT FINAL)
    QML_VALUE_TYPE(messagePart)
public:
    enum PartType { Code, Text };
    Q_ENUM(PartType)

    PartType type;
    QString text;
    QString language;
};

class MessagePartType : public MessagePart
{
    Q_GADGET
};

QML_NAMED_ELEMENT(MessagePart)
QML_FOREIGN_NAMESPACE(QodeAssist::Chat::MessagePartType)
} // namespace QodeAssist::Chat
