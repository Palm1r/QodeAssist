// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QMetaType>
#include <QString>

#include <variant>

#include "session/AgentPlan.hpp"
#include "session/Message.hpp"
#include "session/PermissionRequest.hpp"

namespace QodeAssist::Session {

struct TurnStarted
{
    QString turnId;

    bool operator==(const TurnStarted &other) const = default;
};

struct TextDelta
{
    QString turnId;
    QString text;

    bool operator==(const TextDelta &other) const = default;
};

struct ThinkingReceived
{
    QString turnId;
    QString text;
    QString signature;
    bool redacted = false;

    bool operator==(const ThinkingReceived &other) const = default;
};

struct ToolCallStarted
{
    QString turnId;
    QString toolId;
    QString name;
    QJsonObject arguments;
    bool dropPrecedingText = false;

    bool operator==(const ToolCallStarted &other) const = default;
};

struct ToolCallCompleted
{
    QString turnId;
    QString toolId;
    QString name;
    QString result;

    bool operator==(const ToolCallCompleted &other) const = default;
};

struct ToolCallUpdated
{
    QString turnId;
    QString toolId;
    QString name;
    QString kind;
    QString status;
    QString result;
    QJsonObject details;

    bool operator==(const ToolCallUpdated &other) const = default;
};

struct PlanUpdated
{
    QString turnId;
    QList<PlanEntry> entries;

    bool operator==(const PlanUpdated &other) const = default;
};

struct PermissionRequested
{
    QString turnId;
    QString requestId;
    QString toolCallId;
    QString title;
    QString toolKind;
    QList<PermissionOption> options;

    bool operator==(const PermissionRequested &other) const = default;
};

struct PermissionResolved
{
    QString turnId;
    QString requestId;
    QString optionId;
    bool cancelled = false;

    bool operator==(const PermissionResolved &other) const = default;
};

struct UsageReported
{
    QString turnId;
    Usage usage;

    bool operator==(const UsageReported &other) const = default;
};

struct TurnCompleted
{
    QString turnId;

    bool operator==(const TurnCompleted &other) const = default;
};

struct TurnFailed
{
    QString turnId;
    QString error;

    bool operator==(const TurnFailed &other) const = default;
};

using SessionEvent = std::variant<
    TurnStarted,
    TextDelta,
    ThinkingReceived,
    ToolCallStarted,
    ToolCallCompleted,
    ToolCallUpdated,
    PlanUpdated,
    PermissionRequested,
    PermissionResolved,
    UsageReported,
    TurnCompleted,
    TurnFailed>;

QString turnIdOf(const SessionEvent &event);

} // namespace QodeAssist::Session

Q_DECLARE_METATYPE(QodeAssist::Session::SessionEvent)
