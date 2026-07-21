// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QDebug>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace QodeAssist::Session {

struct PlanEntry
{
    QString content;
    QString priority;
    QString status;

    bool operator==(const PlanEntry &other) const = default;

    friend QDebug operator<<(QDebug debug, const PlanEntry &entry)
    {
        return debug.nospace() << "PlanEntry(" << entry.content << ", " << entry.priority << ", "
                               << entry.status << ")";
    }
};

struct PlanBlock
{
    QList<PlanEntry> entries;

    bool operator==(const PlanBlock &other) const = default;

    friend QDebug operator<<(QDebug debug, const PlanBlock &block)
    {
        return debug.nospace() << "Plan(" << block.entries << ")";
    }
};

QJsonObject planBlockToJson(const PlanBlock &block);
PlanBlock planBlockFromJson(const QJsonObject &json);

QString encodePlanBlock(const PlanBlock &block);
std::optional<PlanBlock> decodePlanBlock(const QString &text);

} // namespace QodeAssist::Session
