// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/TurnLedger.hpp"

#include <QUuid>

#include <utility>

namespace QodeAssist::Session {

TurnLedger::~TurnLedger()
{
    drainPermissions();
}

QString TurnLedger::beginTurn(const QString &turnId)
{
    m_activeTurnId = turnId;
    return m_activeTurnId;
}

QStringList TurnLedger::endTurn()
{
    m_activeTurnId.clear();
    return drainPermissions();
}

QString TurnLedger::activeTurnId() const
{
    return m_activeTurnId;
}

bool TurnLedger::hasActiveTurn() const
{
    return !m_activeTurnId.isEmpty();
}

bool TurnLedger::isActiveTurn(const QString &turnId) const
{
    return !m_activeTurnId.isEmpty() && turnId == m_activeTurnId;
}

QString TurnLedger::registerPermission(PermissionResolver resolve, PermissionCanceller cancel)
{
    const QString requestId
        = QStringLiteral("perm-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_pending.insert(requestId, PendingPermission{std::move(resolve), std::move(cancel)});
    return requestId;
}

bool TurnLedger::resolvePermission(const QString &requestId, const QString &optionId)
{
    const auto it = m_pending.constFind(requestId);
    if (it == m_pending.constEnd())
        return false;

    const PendingPermission pending = it.value();
    m_pending.erase(it);

    if (pending.resolve)
        pending.resolve(optionId);
    return true;
}

bool TurnLedger::cancelPermission(const QString &requestId)
{
    const auto it = m_pending.constFind(requestId);
    if (it == m_pending.constEnd())
        return false;

    const PendingPermission pending = it.value();
    m_pending.erase(it);

    if (pending.cancel)
        pending.cancel();
    return true;
}

QStringList TurnLedger::drainPermissions()
{
    const QHash<QString, PendingPermission> pending = std::exchange(m_pending, {});

    for (const PendingPermission &entry : pending) {
        if (entry.cancel)
            entry.cancel();
    }

    return pending.keys();
}

bool TurnLedger::hasPendingPermission(const QString &requestId) const
{
    return m_pending.contains(requestId);
}

int TurnLedger::pendingPermissionCount() const
{
    return static_cast<int>(m_pending.size());
}

} // namespace QodeAssist::Session
