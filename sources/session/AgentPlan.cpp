// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/AgentPlan.hpp"

#include <QJsonArray>

#include "session/BlockCodec.hpp"

namespace QodeAssist::Session {

QJsonObject planBlockToJson(const PlanBlock &block)
{
    QJsonArray entries;
    for (const PlanEntry &entry : block.entries) {
        QJsonObject json{{"content", entry.content}, {"status", entry.status}};
        if (!entry.priority.isEmpty())
            json["priority"] = entry.priority;
        entries.append(json);
    }

    return QJsonObject{{"entries", entries}};
}

PlanBlock planBlockFromJson(const QJsonObject &json)
{
    PlanBlock block;

    const QJsonArray entries = json["entries"].toArray();
    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        block.entries.append(
            PlanEntry{
                entry["content"].toString(),
                entry["priority"].toString(),
                entry["status"].toString()});
    }

    return block;
}

QString encodePlanBlock(const PlanBlock &block)
{
    return encodeMarkerPayload(planPayloadMarker, planBlockToJson(block));
}

std::optional<PlanBlock> decodePlanBlock(const QString &text)
{
    const auto payload = decodeMarkerPayload(planPayloadMarker, text);
    if (!payload)
        return std::nullopt;

    return planBlockFromJson(*payload);
}

} // namespace QodeAssist::Session
