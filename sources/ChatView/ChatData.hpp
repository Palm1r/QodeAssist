// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QtQmlIntegration>

namespace QodeAssist::Chat {
Q_NAMESPACE
QML_NAMED_ELEMENT(MessagePartType)

enum class MessagePartType { Code, Text, Image };
Q_ENUM_NS(MessagePartType)

} // namespace QodeAssist::Chat
