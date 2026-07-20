// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/PermissionRequest.hpp"

#include <QJsonArray>
#include <QJsonDocument>

namespace QodeAssist::Session {

namespace {

QString permissionMarker()
{
    return QStringLiteral("QODEASSIST_PERMISSION:");
}

bool isPermissionPayload(const QString &text)
{
    return text.startsWith(permissionMarker());
}

} // namespace

QString permissionStatusToString(PermissionStatus status)
{
    switch (status) {
    case PermissionStatus::Pending:
        return QStringLiteral("pending");
    case PermissionStatus::Answered:
        return QStringLiteral("answered");
    case PermissionStatus::Cancelled:
        return QStringLiteral("cancelled");
    }
    return QStringLiteral("pending");
}

PermissionStatus permissionStatusFromString(const QString &status)
{
    if (status == QLatin1String("answered"))
        return PermissionStatus::Answered;
    if (status == QLatin1String("cancelled"))
        return PermissionStatus::Cancelled;
    return PermissionStatus::Pending;
}

std::optional<PermissionOption> PermissionBlock::option(const QString &optionId) const
{
    for (const PermissionOption &candidate : options) {
        if (candidate.id == optionId)
            return candidate;
    }
    return std::nullopt;
}

QJsonObject permissionBlockToJson(const PermissionBlock &block)
{
    QJsonArray options;
    for (const PermissionOption &option : block.options) {
        options.append(
            QJsonObject{{"id", option.id}, {"name", option.name}, {"kind", option.kind}});
    }

    QJsonObject json;
    json["requestId"] = block.requestId;
    json["title"] = block.title;
    json["options"] = options;
    json["status"] = permissionStatusToString(block.status);
    if (!block.toolCallId.isEmpty())
        json["toolCallId"] = block.toolCallId;
    if (!block.toolKind.isEmpty())
        json["toolKind"] = block.toolKind;
    if (!block.selectedOptionId.isEmpty())
        json["selectedOptionId"] = block.selectedOptionId;
    if (block.automatic)
        json["automatic"] = true;

    return json;
}

PermissionBlock permissionBlockFromJson(const QJsonObject &json)
{
    PermissionBlock block;
    block.requestId = json["requestId"].toString();
    block.toolCallId = json["toolCallId"].toString();
    block.title = json["title"].toString();
    block.toolKind = json["toolKind"].toString();
    block.status = permissionStatusFromString(json["status"].toString());
    block.selectedOptionId = json["selectedOptionId"].toString();
    block.automatic = json["automatic"].toBool(false);

    const QJsonArray options = json["options"].toArray();
    for (const QJsonValue &value : options) {
        const QJsonObject option = value.toObject();
        block.options.append(
            PermissionOption{
                option["id"].toString(), option["name"].toString(), option["kind"].toString()});
    }

    return block;
}

PermissionBlock restoredPermissionBlock(PermissionBlock block)
{
    if (block.status == PermissionStatus::Pending)
        block.status = PermissionStatus::Cancelled;
    return block;
}

QString encodePermissionBlock(const PermissionBlock &block)
{
    return permissionMarker()
           + QString::fromUtf8(
               QJsonDocument(permissionBlockToJson(block)).toJson(QJsonDocument::Compact));
}

std::optional<PermissionBlock> decodePermissionBlock(const QString &text)
{
    if (!isPermissionPayload(text))
        return std::nullopt;

    const QJsonDocument document
        = QJsonDocument::fromJson(text.mid(permissionMarker().size()).toUtf8());
    if (!document.isObject())
        return std::nullopt;

    return permissionBlockFromJson(document.object());
}

} // namespace QodeAssist::Session
