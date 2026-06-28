// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "SessionFileRegistry.hpp"

#include <utility>

#include <QFileInfo>

#include <Session.hpp>

namespace QodeAssist::Chat {

SessionFileRegistry::SessionFileRegistry(QObject *parent)
    : QObject(parent)
{}

SessionFileRegistry::~SessionFileRegistry() = default;

bool SessionFileRegistry::isLocked(const QString &path) const
{
    return !path.isEmpty() && !m_locks.value(path).isNull();
}

bool SessionFileRegistry::isLockedByOther(const QString &path, QodeAssist::Session *self) const
{
    if (path.isEmpty())
        return false;
    const auto owner = m_locks.value(path);
    return !owner.isNull() && owner != self;
}

bool SessionFileRegistry::lock(const QString &path, QodeAssist::Session *owner)
{
    if (path.isEmpty())
        return false;
    const auto existing = m_locks.value(path);
    if (!existing.isNull() && existing != owner)
        return false;
    m_locks.insert(path, owner);
    return true;
}

void SessionFileRegistry::release(const QString &path)
{
    m_locks.remove(path);
}

void SessionFileRegistry::setPendingChatFile(const QString &path)
{
    m_pendingChatFile = path;
}

QString SessionFileRegistry::takePendingChatFile()
{
    return std::exchange(m_pendingChatFile, QString{});
}

QString SessionFileRegistry::uniqueFreePath(const QString &desiredPath) const
{
    if (desiredPath.isEmpty() || !isLocked(desiredPath)) {
        return desiredPath;
    }

    const QFileInfo info(desiredPath);
    const QString dir = info.path();
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();

    for (int counter = 2;; ++counter) {
        QString candidate = dir + '/' + base + '_' + QString::number(counter);
        if (!suffix.isEmpty()) {
            candidate += '.' + suffix;
        }
        if (!isLocked(candidate)) {
            return candidate;
        }
    }
}

} // namespace QodeAssist::Chat
