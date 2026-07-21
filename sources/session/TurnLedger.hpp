// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>

namespace QodeAssist::Session {

class TurnLedger
{
public:
    using PermissionResolver = std::function<void(const QString &optionId)>;
    using PermissionCanceller = std::function<void()>;

    TurnLedger() = default;
    ~TurnLedger();
    TurnLedger(const TurnLedger &) = delete;
    TurnLedger &operator=(const TurnLedger &) = delete;

    QString beginTurn(const QString &turnId);
    QStringList endTurn();
    QString activeTurnId() const;
    bool hasActiveTurn() const;
    bool isActiveTurn(const QString &turnId) const;

    QString registerPermission(PermissionResolver resolve, PermissionCanceller cancel);
    bool resolvePermission(const QString &requestId, const QString &optionId);
    bool cancelPermission(const QString &requestId);
    QStringList drainPermissions();
    bool hasPendingPermission(const QString &requestId) const;
    int pendingPermissionCount() const;

private:
    struct PendingPermission
    {
        PermissionResolver resolve;
        PermissionCanceller cancel;
    };

    QString m_activeTurnId;
    QHash<QString, PendingPermission> m_pending;
};

} // namespace QodeAssist::Session
