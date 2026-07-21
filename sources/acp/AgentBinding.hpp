// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace QodeAssist::Acp {

inline constexpr qsizetype maxAgentBindingIdLength = 256;

struct AgentBinding
{
    QString agentId;
    QString sessionId;

    bool isEmpty() const { return agentId.isEmpty(); }

    QString displayId() const;

    bool operator==(const AgentBinding &other) const = default;

    QJsonObject toJson() const;
    static AgentBinding fromJson(const QJsonValue &value, QString *error = nullptr);
};

} // namespace QodeAssist::Acp
