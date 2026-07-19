// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

#include "session/ConversationHistory.hpp"

namespace QodeAssist::Session {

class HistorySerializer
{
public:
    static QString currentVersion();
    static bool isSupportedVersion(const QString &version);

    static QJsonObject toJson(const ConversationHistory &history);
    static std::optional<ConversationHistory> fromJson(const QJsonObject &root);
};

} // namespace QodeAssist::Session
