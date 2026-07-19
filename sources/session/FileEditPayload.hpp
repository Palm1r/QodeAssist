// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

namespace QodeAssist::Session {

bool isFileEditPayload(const QString &text);
std::optional<QJsonObject> parseFileEditPayload(const QString &text);
QString encodeFileEditPayload(const QJsonObject &payload);

} // namespace QodeAssist::Session
