// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>

#include "Message.hpp"

namespace QodeAssist {

class MessageSerializer
{
public:
    static QJsonObject toJson(const Message &message);

    static Message fromJson(const QJsonObject &json, bool *ok = nullptr);
};

} // namespace QodeAssist
