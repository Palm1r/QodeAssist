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

namespace PermissionOptionKind {
inline constexpr QLatin1StringView AllowOnce{"allow_once"};
inline constexpr QLatin1StringView AllowAlways{"allow_always"};
inline constexpr QLatin1StringView RejectOnce{"reject_once"};
inline constexpr QLatin1StringView RejectAlways{"reject_always"};
} // namespace PermissionOptionKind

struct PermissionOption
{
    QString id;
    QString name;
    QString kind;

    bool allows() const
    {
        return kind == PermissionOptionKind::AllowOnce || kind == PermissionOptionKind::AllowAlways;
    }

    bool scopesToConversation() const
    {
        return kind == PermissionOptionKind::AllowAlways
               || kind == PermissionOptionKind::RejectAlways;
    }

    bool operator==(const PermissionOption &other) const = default;

    friend QDebug operator<<(QDebug debug, const PermissionOption &option)
    {
        return debug.nospace() << "Option(" << option.id << ", " << option.name << ", "
                               << option.kind << ")";
    }
};

enum class PermissionStatus { Pending, Answered, Cancelled };

QString permissionStatusToString(PermissionStatus status);
PermissionStatus permissionStatusFromString(const QString &status);

struct PermissionBlock
{
    QString requestId;
    QString toolCallId;
    QString title;
    QString toolKind;
    QList<PermissionOption> options;
    PermissionStatus status = PermissionStatus::Pending;
    QString selectedOptionId;
    bool automatic = false;

    std::optional<PermissionOption> option(const QString &optionId) const;

    bool operator==(const PermissionBlock &other) const = default;

    friend QDebug operator<<(QDebug debug, const PermissionBlock &block)
    {
        return debug.nospace() << "Permission(" << block.requestId << ", " << block.title
                               << ", kind=" << block.toolKind << ", options=" << block.options
                               << ", status=" << permissionStatusToString(block.status)
                               << ", selected=" << block.selectedOptionId
                               << ", automatic=" << block.automatic << ")";
    }
};

QJsonObject permissionBlockToJson(const PermissionBlock &block);
PermissionBlock permissionBlockFromJson(const QJsonObject &json);

PermissionBlock restoredPermissionBlock(PermissionBlock block);

QString encodePermissionBlock(const PermissionBlock &block);
std::optional<PermissionBlock> decodePermissionBlock(const QString &text);

} // namespace QodeAssist::Session
