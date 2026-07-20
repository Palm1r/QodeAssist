// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/AgentPlan.hpp"

#include <QJsonArray>
#include <QJsonDocument>

namespace QodeAssist::Session {

namespace {

QString planMarker()
{
    return QStringLiteral("QODEASSIST_PLAN:");
}

} // namespace

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
    return planMarker()
           + QString::fromUtf8(QJsonDocument(planBlockToJson(block)).toJson(QJsonDocument::Compact));
}

std::optional<PlanBlock> decodePlanBlock(const QString &text)
{
    if (!text.startsWith(planMarker()))
        return std::nullopt;

    const QJsonDocument document = QJsonDocument::fromJson(text.mid(planMarker().size()).toUtf8());
    if (!document.isObject())
        return std::nullopt;

    return planBlockFromJson(document.object());
}

} // namespace QodeAssist::Session
